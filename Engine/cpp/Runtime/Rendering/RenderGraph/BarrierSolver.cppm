// Draconic::RenderGraph - :barrier_solver partition
//
// Computes and emits resource barriers between passes. State is tracked at two
// levels: per-resource-handle (buffers; convenience for textures) and per-
// Texture with per-subresource granularity (source of truth for textures, keyed
// by the GPU texture pointer so the same texture unifies across handles). Ported
// from Sedulous.RenderGraph (BarrierSolver.bf).

module;

#include <algorithm>

#include <vector>
#include <span>
#include <unordered_map>

export module rendergraph:barrier_solver;

import core;
import rhi;
import :types;
import :resource;
import :pass;
import :state_tracker;

using namespace draco;

export namespace draco::rendergraph
{
    namespace rhi = draco::rhi;

    class BarrierSolver
    {
    public:
        BarrierSolver() = default;
        ~BarrierSolver() { clearTrackers(); }

        BarrierSolver(const BarrierSolver&) = delete;
        BarrierSolver& operator=(const BarrierSolver&) = delete;

        // Initialize states from the resource list (call once per frame). Persistent
        // resources use their last-known state; transient resources start Undefined.
        void reset(std::span<RenderGraphResource* const> resources)
        {
            m_resourceStates.clear();
            clearTrackers();

            for (i32 i = 0; i < static_cast<i32>(resources.size()); ++i)
            {
                RenderGraphResource* res = resources[static_cast<usize>(i)];
                if (res == nullptr) { continue; }

                rhi::ResourceState initialState = rhi::ResourceState::Undefined;

                if (res->lifetime == RGResourceLifetime::Persistent && res->persistentData.get() != nullptr)
                {
                    initialState = res->persistentData->firstFrame
                        ? (res->texture != nullptr ? res->texture->initialState : rhi::ResourceState::Undefined)
                        : res->persistentData->lastKnownState;
                }
                else if (res->lifetime == RGResourceLifetime::Imported)
                {
                    initialState = res->lastKnownState;
                }
                else
                {
                    // Transient: always start Undefined; the backend uses the texture's
                    // actual tracked state for the "before" side, so first access always
                    // gets a correct transition.
                    initialState = rhi::ResourceState::Undefined;
                }

                m_resourceStates.insert_or_assign(i, initialState);

                if (res->resourceType == RGResourceType::Texture && res->texture != nullptr)
                {
                    if (SubresourceStateTracker** existing = mapFind(m_textureStates, res->texture))
                    {
                        // Same GPU texture via another handle - unify.
                        SubresourceStateTracker* tracker = *existing;
                        if (initialState == rhi::ResourceState::Undefined && tracker->isUniform()
                            && tracker->uniformState() != rhi::ResourceState::Undefined)
                        {
                            m_resourceStates.insert_or_assign(i, tracker->uniformState());
                        }
                        else if (initialState != rhi::ResourceState::Undefined)
                        {
                            tracker->setAll(initialState);
                        }
                    }
                    else
                    {
                        const u32 mipCount = res->texture->desc.mipLevelCount;
                        const u32 layerCount = res->texture->desc.arrayLayerCount;
                        SubresourceStateTracker* tracker =
                            new SubresourceStateTracker(mipCount, layerCount, initialState);

                        if (res->lifetime == RGResourceLifetime::Persistent && res->persistentData.get() != nullptr
                            && !res->persistentData->firstFrame
                            && !res->persistentData->subresourceStates.empty())
                        {
                            tracker->initFromStates(res->persistentData->subresourceStates, initialState);
                        }

                        m_textureStates.insert_or_assign(res->texture, tracker);
                    }
                }
            }
        }

        // Emit barriers needed before executing `pass`.
        void emitBarriers(const RenderGraphPass& pass, std::span<RenderGraphResource* const> resources,
                          rhi::CommandEncoder& encoder)
        {
            m_textureBarriers.clear();
            m_bufferBarriers.clear();

            for (const RGResourceAccess& access : pass.accesses)
            {
                if (!access.handle.isValid()) { continue; }
                const i32 resIdx = static_cast<i32>(access.handle.index);
                if (static_cast<usize>(resIdx) >= resources.size()) { continue; }

                RenderGraphResource* res = resources[static_cast<usize>(resIdx)];
                if (res == nullptr) { continue; }

                const rhi::ResourceState requiredState = access.toResourceState();
                const bool accessIsReadWrite = isRead(access.type) && isWrite(access.type);

                if (res->resourceType == RGResourceType::Texture && res->texture != nullptr)
                {
                    SubresourceStateTracker** found = mapFind(m_textureStates, res->texture);
                    if (found == nullptr) { continue; }
                    SubresourceStateTracker* tracker = *found;

                    emitTextureBarriers(*tracker, res->texture, access.subresource, requiredState, accessIsReadWrite);
                    tracker->setState(access.subresource, requiredState);
                    m_resourceStates.insert_or_assign(resIdx, requiredState);
                }
                else if (res->resourceType == RGResourceType::Buffer && res->buffer != nullptr)
                {
                    rhi::ResourceState currentState = rhi::ResourceState::Undefined;
                    if (rhi::ResourceState* p = mapFind(m_resourceStates, resIdx)) { currentState = *p; }
                    if (currentState == requiredState) { continue; }

                    rhi::BufferBarrier bb{};
                    bb.buffer = res->buffer;
                    bb.oldState = currentState;
                    bb.newState = requiredState;
                    m_bufferBarriers.push_back(bb);

                    m_resourceStates.insert_or_assign(resIdx, requiredState);
                }
            }

            flushBarriers(encoder);
        }

        // After a pass executes: transition resources marked ReadableAfterWrite
        // (written by the pass) to ShaderRead so external bind groups can sample them.
        void emitReadableAfterWriteBarriers(const RenderGraphPass& pass,
                                            std::span<RenderGraphResource* const> resources,
                                            rhi::CommandEncoder& encoder)
        {
            m_textureBarriers.clear();
            m_bufferBarriers.clear();

            for (const RGResourceAccess& access : pass.accesses)
            {
                if (!access.isWrite() || !access.handle.isValid()) { continue; }
                const i32 resIdx = static_cast<i32>(access.handle.index);
                if (static_cast<usize>(resIdx) >= resources.size()) { continue; }

                RenderGraphResource* res = resources[static_cast<usize>(resIdx)];
                if (res == nullptr || !res->readableAfterWrite) { continue; }
                if (res->resourceType != RGResourceType::Texture || res->texture == nullptr) { continue; }

                SubresourceStateTracker** found = mapFind(m_textureStates, res->texture);
                if (found == nullptr) { continue; }
                SubresourceStateTracker* tracker = *found;

                emitTextureBarriers(*tracker, res->texture, access.subresource, rhi::ResourceState::ShaderRead, false);
                tracker->setState(access.subresource, rhi::ResourceState::ShaderRead);
                m_resourceStates.insert_or_assign(resIdx, rhi::ResourceState::ShaderRead);
            }

            flushBarriers(encoder);
        }

        // Transition imported resources to their requested final state.
        void emitFinalTransitions(std::span<RenderGraphResource* const> resources, rhi::CommandEncoder& encoder)
        {
            m_textureBarriers.clear();
            m_bufferBarriers.clear();

            for (i32 i = 0; i < static_cast<i32>(resources.size()); ++i)
            {
                RenderGraphResource* res = resources[static_cast<usize>(i)];
                if (res == nullptr || !res->finalState.has_value()) { continue; }

                const rhi::ResourceState finalState = res->finalState.value();
                if (res->texture != nullptr)
                {
                    SubresourceStateTracker** found = mapFind(m_textureStates, res->texture);
                    if (found == nullptr) { continue; }
                    SubresourceStateTracker* tracker = *found;

                    emitTextureBarriers(*tracker, res->texture, RGSubresourceRange::all(), finalState, false);
                    tracker->setAll(finalState);
                    m_resourceStates.insert_or_assign(i, finalState);
                }
            }

            flushBarriers(encoder);
        }

        // Write tracked states back into persistent/imported resources for next frame.
        void updatePersistentStates(std::span<RenderGraphResource* const> resources)
        {
            for (i32 i = 0; i < static_cast<i32>(resources.size()); ++i)
            {
                RenderGraphResource* res = resources[static_cast<usize>(i)];
                if (res == nullptr) { continue; }

                if (res->resourceType == RGResourceType::Texture && res->texture != nullptr)
                {
                    SubresourceStateTracker** found = mapFind(m_textureStates, res->texture);
                    if (found == nullptr) { continue; }
                    SubresourceStateTracker* tracker = *found;

                    if (tracker->isUniform())
                    {
                        res->lastKnownState = tracker->uniformState();
                        if (res->persistentData.get() != nullptr)
                        {
                            res->persistentData->lastKnownState = tracker->uniformState();
                            res->persistentData->firstFrame = false;
                            res->persistentData->subresourceStates.clear();
                        }
                    }
                    else
                    {
                        res->lastKnownState = tracker->getState(0, 0);
                        if (res->persistentData.get() != nullptr)
                        {
                            res->persistentData->lastKnownState = tracker->getState(0, 0);
                            res->persistentData->firstFrame = false;
                            res->persistentData->subresourceStates = tracker->copyStates();
                        }
                    }
                }
                else if (rhi::ResourceState* p = mapFind(m_resourceStates, i))
                {
                    res->lastKnownState = *p;
                    if (res->persistentData.get() != nullptr)
                    {
                        res->persistentData->lastKnownState = *p;
                        res->persistentData->firstFrame = false;
                    }
                }
            }
        }

        [[nodiscard]] rhi::ResourceState getState(i32 resourceIndex)
        {
            if (rhi::ResourceState* p = mapFind(m_resourceStates, resourceIndex)) { return *p; }
            return rhi::ResourceState::Undefined;
        }

        [[nodiscard]] rhi::ResourceState getTextureState(rhi::Texture* texture)
        {
            if (texture == nullptr) { return rhi::ResourceState::Undefined; }
            if (SubresourceStateTracker** found = mapFind(m_textureStates, texture))
            {
                SubresourceStateTracker* tracker = *found;
                return tracker->isUniform() ? tracker->uniformState() : tracker->getState(0, 0);
            }
            return rhi::ResourceState::Undefined;
        }

        [[nodiscard]] SubresourceStateTracker* getTextureTracker(rhi::Texture* texture)
        {
            if (texture == nullptr) { return nullptr; }
            SubresourceStateTracker** found = mapFind(m_textureStates, texture);
            return found != nullptr ? *found : nullptr;
        }

    private:
        void emitTextureBarriers(SubresourceStateTracker& tracker, rhi::Texture* texture,
                                 RGSubresourceRange subresource, rhi::ResourceState requiredState,
                                 bool accessIsReadWrite)
        {
            const u32 totalMips = tracker.mipCount();
            const u32 totalLayers = tracker.layerCount();

            if (tracker.isUniform())
            {
                const rhi::ResourceState currentState = tracker.uniformState();
                if (currentState == requiredState && !accessIsReadWrite) { return; }

                rhi::TextureBarrier barrier{};
                barrier.texture = texture;
                barrier.oldState = currentState;
                barrier.newState = requiredState;
                if (!subresource.isAll())
                {
                    barrier.baseMipLevel = subresource.baseMipLevel;
                    barrier.mipLevelCount = subresource.mipLevelCount == 0 ? 0xFFFFFFFFu : subresource.mipLevelCount;
                    barrier.baseArrayLayer = subresource.baseArrayLayer;
                    barrier.arrayLayerCount = subresource.arrayLayerCount == 0 ? 0xFFFFFFFFu : subresource.arrayLayerCount;
                }
                m_textureBarriers.push_back(barrier);
            }
            else
            {
                const u32 baseMip = subresource.baseMipLevel;
                const u32 mipEnd = subresource.mipLevelCount == 0 ? totalMips
                                                                  : std::min(baseMip + subresource.mipLevelCount, totalMips);
                const u32 baseLayer = subresource.baseArrayLayer;
                const u32 layerEnd = subresource.arrayLayerCount == 0 ? totalLayers
                                                                      : std::min(baseLayer + subresource.arrayLayerCount, totalLayers);

                for (u32 layer = baseLayer; layer < layerEnd; ++layer)
                {
                    for (u32 mip = baseMip; mip < mipEnd; ++mip)
                    {
                        const rhi::ResourceState currentState = tracker.getState(mip, layer);
                        if (currentState == requiredState && !accessIsReadWrite) { continue; }

                        rhi::TextureBarrier barrier{};
                        barrier.texture = texture;
                        barrier.oldState = currentState;
                        barrier.newState = requiredState;
                        barrier.baseMipLevel = mip;
                        barrier.mipLevelCount = 1;
                        barrier.baseArrayLayer = layer;
                        barrier.arrayLayerCount = 1;
                        m_textureBarriers.push_back(barrier);
                    }
                }
            }
        }

        void flushBarriers(rhi::CommandEncoder& encoder)
        {
            if (m_textureBarriers.empty() && m_bufferBarriers.empty()) { return; }

            rhi::BarrierGroup group{};
            if (!m_textureBarriers.empty())
            {
                group.textureBarriers =
                    std::span<const rhi::TextureBarrier>(m_textureBarriers.data(), m_textureBarriers.size());
            }
            if (!m_bufferBarriers.empty())
            {
                group.bufferBarriers =
                    std::span<const rhi::BufferBarrier>(m_bufferBarriers.data(), m_bufferBarriers.size());
            }
            encoder.barrier(group);
        }

        void clearTrackers()
        {
            for (auto& entry : m_textureStates)
            {
                delete (entry.second);
            }
            m_textureStates.clear();
        }

        std::unordered_map<i32, rhi::ResourceState> m_resourceStates;
        std::unordered_map<rhi::Texture*, SubresourceStateTracker*> m_textureStates;
        std::vector<rhi::TextureBarrier> m_textureBarriers;
        std::vector<rhi::BufferBarrier> m_bufferBarriers;
    };
}
