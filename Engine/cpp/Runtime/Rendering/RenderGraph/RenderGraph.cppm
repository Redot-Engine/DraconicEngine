// Draconic::RenderGraph - :graph partition
//
// The orchestrator. GPU work is declared as passes with resource accesses; the
// graph builds dependencies, culls unused work, topologically sorts, allocates/
// aliases transient resources, inserts barriers (via BarrierSolver), and runs
// (RenderGraph.bf). compile() works without an encoder so graph logic is
// testable headless.

module;

#include <chrono>
#include <vector>
#include <span>
#include <optional>
#include <memory>
#include <string>
#include <string_view>

export module rendergraph:graph;

import core;
import rhi;
import :types;
import :descriptors;
import :callbacks;
import :resource;
import :persistent_resource;
import :pass;
import :pass_builder;
import :barrier_solver;
import :profiler;
import :transient_pool;

using namespace draco;

export namespace draco::rendergraph
{
    namespace rhi = draco::rhi;

    // NOTE: local stand-in. Replace with a core timing/clock utility once draco.core
    // provides one (Sedulous's core::GetTicks).
    // Monotonic CPU tick counter for the profiler (nanoseconds since the epoch).
    [[nodiscard]] inline u64 getTicks() noexcept
    {
        return static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    // getTicks() deltas are nanoseconds (steady_clock); convert to milliseconds for reports.
    [[nodiscard]] inline f32 ticksToMilliseconds(u64 ticks) noexcept
    {
        return static_cast<f32>(ticks) / 1000000.0f;
    }

    class RenderGraph
    {
    public:
        explicit RenderGraph(rhi::Device* device, RenderGraphConfig config = {})
            : m_device(device), m_config(config)
        {
            if (device != nullptr) { m_texturePool = std::make_unique<TransientTexturePool>(*device); }
            const i32 slots = config.frameBufferCount > 0 ? config.frameBufferCount : 1;
            for (i32 i = 0; i < slots; ++i) { m_deferredDeletions.push_back(std::vector<DeferredDeletion>{}); }
        }

        // Turn on per-pass GPU timestamp profiling (lazy; needs the device). Idempotent.
        void enableGpuProfiling()
        {
            if (m_gpuProfiler.get() != nullptr || m_device == nullptr) { return; }
            m_gpuProfiler = std::make_unique<GraphProfiler>();
            if (!m_gpuProfiler->init(*m_device).isOk()) { m_gpuProfiler.reset(); return; }
            if (rhi::Queue* q = m_device->getQueue(rhi::QueueType::Graphics)) { m_gpuProfiler->setTimestampPeriod(q->timestampPeriod()); }
        }

        // The GPU profiler (null if not enabled). Read its results after the GPU has finished (e.g.
        // after WaitIdle on a P-key dump). `LastProfiledPassCount` is how many passes were timed.
        [[nodiscard]] GraphProfiler* gpuProfiler() noexcept { return m_gpuProfiler.get(); }
        [[nodiscard]] i32 lastProfiledPassCount() const noexcept { return m_lastProfiledPassCount; }

        // Aggregate this frame's per-pass CPU RECORD time by pass name (most-expensive first). The graph
        // execute is often CPU-bound (recording/bundle build) while the GPU is idle - this shows where.
        void appendCpuPassReport(std::u8string& out) const {
            struct Agg { std::u8string_view name; u64 ticks = 0; i32 n = 0; };
            std::vector<Agg> agg; u64 total = 0;
            for (const PassCpu& p : m_passCpu) {
                total += p.ticks;
                bool found = false;
                for (Agg& a : agg) { if (a.name == p.name) { a.ticks += p.ticks; ++a.n; found = true; break; } }
                if (!found) { Agg a; a.name = p.name; a.ticks = p.ticks; a.n = 1; agg.push_back(a); }
            }
            for (usize i = 0; i < agg.size(); ++i)
                for (usize j = i + 1; j < agg.size(); ++j)
                    if (agg[j].ticks > agg[i].ticks) { const Agg t = agg[i]; agg[i] = agg[j]; agg[j] = t; }
            out.append(u8"=== CPU by pass name (record cost, expensive first) ===\n");
            for (const Agg& a : agg) { appendFormat(out, u8"  {} ms  (x{})  {}\n", ticksToMilliseconds(a.ticks), a.n, a.name); }
            appendFormat(out, u8"  --------\n  {} ms  TOTAL (pass record)\n", ticksToMilliseconds(total));
        }

        ~RenderGraph()
        {
            for (std::vector<DeferredDeletion>& list : m_deferredDeletions)
            {
                for (DeferredDeletion& d : list) { d.execute(m_device); }
            }
            for (RenderGraphPass* pass : m_passes) { delete (pass); }
            for (RenderGraphResource* res : m_resources) { if (res != nullptr) { delete (res); } }
        }

        RenderGraph(const RenderGraph&) = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;

        // --- output dimensions (for SizeMode resolution) ---
        void setOutputSize(u32 width, u32 height) noexcept { m_outputWidth = width; m_outputHeight = height; }
        [[nodiscard]] u32 outputWidth() const noexcept { return m_outputWidth; }
        [[nodiscard]] u32 outputHeight() const noexcept { return m_outputHeight; }

        // --- frame lifecycle ---
        void beginFrame(i32 frameIndex)
        {
            m_frameIndex = frameIndex;
            flushDeferred(frameIndex);
            clearPasses();
            recycleNonPersistent();
            m_isCompiled = false;
        }

        // Compile only (cull, sort, allocate). Safe without an encoder (tests).
        [[nodiscard]] Status compile()
        {
            if (m_passes.empty()) { return Status{}; }

            buildResourceReferences();
            cullPasses();
            buildDependencies();
            if (!topologicalSort().isOk()) { return Status{ ErrorCode::Unknown }; }
            allocateTransientResources();

            m_isCompiled = true;
            return Status{};
        }

        // Compile (if needed) and execute into `encoder` (null = compile-only).
        [[nodiscard]] Status execute(rhi::CommandEncoder* encoder)
        {
            if (!m_isCompiled)
            {
                if (!compile().isOk()) { return Status{ ErrorCode::Unknown }; }
            }
            if (encoder == nullptr) { return Status{}; }

            m_barrierSolver.reset(resourceSpan());

            // GPU profiling: reset the timestamp pool up front (must be outside any render pass),
            // then bracket each executed pass with begin/end timestamps.
            const bool prof = (m_gpuProfiler.get() != nullptr);
            if (prof) { m_gpuProfiler->beginFrame(*encoder); m_passCpu.clear(); }
            i32 profiledPassCount = 0;

            for (i32 passIdx : m_executionOrder)
            {
                RenderGraphPass* pass = m_passes[static_cast<usize>(passIdx)];
                if (pass->isCulled) { continue; }
                if (static_cast<bool>(pass->condition) && !pass->condition()) { continue; }

                // Per-pass CPU cost (barriers + record/execute, incl. any bundle build/wait): the graph's
                // execute is often CPU-bound while the GPU is idle, so this shows which pass RECORDING is slow.
                const u64 cpuStart = prof ? getTicks() : 0;
                encoder->beginDebugLabel(pass->name);
                if (prof) { m_gpuProfiler->beginPass(*encoder, profiledPassCount, pass->name); }
                m_barrierSolver.emitBarriers(*pass, resourceSpan(), *encoder);

                switch (pass->type)
                {
                    case RGPassType::Render:  executeRenderPass(*pass, *encoder); break;
                    case RGPassType::Compute: executeComputePass(*pass, *encoder); break;
                    case RGPassType::Copy:    executeCopyPass(*pass, *encoder); break;
                }

                m_barrierSolver.emitReadableAfterWriteBarriers(*pass, resourceSpan(), *encoder);
                if (prof) { m_gpuProfiler->endPass(*encoder, profiledPassCount); ++profiledPassCount; }
                encoder->endDebugLabel();
                if (prof) { m_passCpu.push_back(PassCpu{ pass->name, getTicks() - cpuStart }); }
            }

            if (m_gpuProfiler.get() != nullptr) { m_gpuProfiler->resolve(*encoder, profiledPassCount); m_lastProfiledPassCount = profiledPassCount; }

            m_barrierSolver.emitFinalTransitions(resourceSpan(), *encoder);
            m_barrierSolver.updatePersistentStates(resourceSpan());

            // Defer-delete per-subresource views created this frame.
            if (!m_subresourceViews.empty())
            {
                std::vector<DeferredDeletion>& deletions = deferredSlot();
                for (rhi::TextureView* view : m_subresourceViews)
                {
                    DeferredDeletion d{}; d.view = view; deletions.push_back(d);
                }
                m_subresourceViews.clear();
            }

            returnTransientResources();
            return Status{};
        }

        void endFrame()
        {
            m_isCompiled = false;
            if (m_texturePool.get() != nullptr) { m_texturePool->endFrame(); }
        }

        // Clear passes but keep persistent resource state (multi-view rendering).
        void reset()
        {
            returnTransientResources();
            clearPasses();
            recycleNonPersistent();
            m_isCompiled = false;
        }

        // --- resource creation ---
        RGHandle createTransient(std::u8string_view name, RGTextureDesc desc)
        {
            RenderGraphResource* res = new RenderGraphResource(name, RGResourceType::Texture, RGResourceLifetime::Transient);
            desc.resolve(m_outputWidth, m_outputHeight);
            res->textureDesc = desc;
            return addResource(res);
        }

        RGHandle createTransientBuffer(std::u8string_view name, RGBufferDesc desc)
        {
            RenderGraphResource* res = new RenderGraphResource(name, RGResourceType::Buffer, RGResourceLifetime::Transient);
            res->bufferDesc = desc;
            return addResource(res);
        }

        RGHandle registerPersistent(std::u8string_view name, rhi::Texture* texture, rhi::TextureView* view)
        {
            RenderGraphResource* res = new RenderGraphResource(name, RGResourceType::Texture, RGResourceLifetime::Persistent);
            res->texture = texture;
            res->textureView = view;
            res->persistentData = std::make_unique<PersistentResource>(texture, view);
            return addResource(res);
        }

        RGHandle registerPersistentPingPong(std::u8string_view name, rhi::Texture* tex0, rhi::Texture* tex1,
                                            rhi::TextureView* view0, rhi::TextureView* view1)
        {
            RenderGraphResource* res = new RenderGraphResource(name, RGResourceType::Texture, RGResourceLifetime::Persistent);
            res->texture = tex0;
            res->textureView = view0;
            res->persistentData = std::make_unique<PersistentResource>(tex0, tex1, view0, view1);
            return addResource(res);
        }

        RGHandle importTarget(std::u8string_view name, rhi::Texture* texture, rhi::TextureView* view,
                              std::optional<rhi::ResourceState> finalState = {},
                              std::optional<rhi::ResourceState> currentState = {})
        {
            RenderGraphResource* res = new RenderGraphResource(name, RGResourceType::Texture, RGResourceLifetime::Imported);
            res->texture = texture;
            res->textureView = view;
            res->finalState = finalState;
            res->lastKnownState = currentState.has_value() ? currentState.value()
                                : (texture != nullptr ? texture->initialState : rhi::ResourceState::Undefined);
            return addResource(res);
        }

        // Depth import variant carrying a depth-only view for shader sampling.
        RGHandle importTarget(std::u8string_view name, rhi::Texture* texture, rhi::TextureView* view,
                              rhi::TextureView* depthOnlyView,
                              std::optional<rhi::ResourceState> finalState = {},
                              std::optional<rhi::ResourceState> currentState = {})
        {
            RenderGraphResource* res = new RenderGraphResource(name, RGResourceType::Texture, RGResourceLifetime::Imported);
            res->texture = texture;
            res->textureView = view;
            res->depthOnlyView = depthOnlyView;
            res->finalState = finalState;
            res->lastKnownState = currentState.has_value() ? currentState.value()
                                : (texture != nullptr ? texture->initialState : rhi::ResourceState::Undefined);
            return addResource(res);
        }

        RGHandle importBuffer(std::u8string_view name, rhi::Buffer* buffer)
        {
            RenderGraphResource* res = new RenderGraphResource(name, RGResourceType::Buffer, RGResourceLifetime::Imported);
            res->buffer = buffer;
            return addResource(res);
        }

        void requireReadableAfterWrite(RGHandle handle)
        {
            if (RenderGraphResource* res = resolve(handle)) { res->readableAfterWrite = true; }
        }

        // --- pass creation (setup callable receives PassBuilder&) ---
        template <typename Setup>
        PassHandle addRenderPass(std::u8string_view name, Setup&& setup) { return addPassOfType(name, RGPassType::Render, setup); }
        template <typename Setup>
        PassHandle addComputePass(std::u8string_view name, Setup&& setup) { return addPassOfType(name, RGPassType::Compute, setup); }
        template <typename Setup>
        PassHandle addCopyPass(std::u8string_view name, Setup&& setup) { return addPassOfType(name, RGPassType::Copy, setup); }

        // --- resource access (during execute callbacks) ---
        [[nodiscard]] rhi::Texture* getTexture(RGHandle handle)
        {
            RenderGraphResource* res = resolveChecked(handle);
            if (res == nullptr) { return nullptr; }
            return res->persistentData.get() != nullptr ? res->persistentData->currentTexture() : res->texture;
        }
        [[nodiscard]] rhi::TextureView* getTextureView(RGHandle handle)
        {
            RenderGraphResource* res = resolveChecked(handle);
            if (res == nullptr) { return nullptr; }
            return res->persistentData.get() != nullptr ? res->persistentData->currentView() : res->textureView;
        }

        // A stable id for the GPU texture currently backing `handle`. For a transient it changes when
        // the graph (re)allocates a different physical texture (e.g. on resize) - even if the new view
        // happens to reuse a freed address. A bind-group cache over GetTextureView MUST also key on
        // this to stay correct across resizes. 0 when unresolved.
        [[nodiscard]] u64 getTextureGeneration(RGHandle handle)
        {
            RenderGraphResource* res = resolveChecked(handle);
            return res != nullptr ? res->textureGeneration : 0;
        }
        [[nodiscard]] rhi::TextureView* getDepthOnlyTextureView(RGHandle handle)
        {
            RenderGraphResource* res = resolveChecked(handle);
            return res != nullptr ? res->depthOnlyView : nullptr;
        }
        [[nodiscard]] rhi::Buffer* getBuffer(RGHandle handle)
        {
            RenderGraphResource* res = resolveChecked(handle);
            return res != nullptr ? res->buffer : nullptr;
        }

        void swapPingPong(RGHandle handle)
        {
            RenderGraphResource* res = resolve(handle);
            if (res != nullptr && res->persistentData.get() != nullptr)
            {
                res->persistentData->swap();
                res->texture = res->persistentData->currentTexture();
                res->textureView = res->persistentData->currentView();
            }
        }

        // --- queries ---
        [[nodiscard]] usize passCount() const noexcept { return m_passes.size(); }
        [[nodiscard]] usize resourceCount() const noexcept
        {
            usize count = 0;
            for (RenderGraphResource* r : m_resources) { if (r != nullptr) { ++count; } }
            return count;
        }
        [[nodiscard]] usize culledPassCount() const noexcept
        {
            usize count = 0;
            for (RenderGraphPass* p : m_passes) { if (p->isCulled) { ++count; } }
            return count;
        }
        [[nodiscard]] RGHandle getResource(std::u8string_view name) const
        {
            for (usize i = 0; i < m_resources.size(); ++i)
            {
                RenderGraphResource* res = m_resources[i];
                if (res != nullptr && res->name == name) { return RGHandle{ static_cast<u32>(i), res->generation }; }
            }
            return RGHandle::invalid();
        }
        [[nodiscard]] rhi::ResourceState getResourceState(RGHandle handle)
        {
            RenderGraphResource* res = resolveChecked(handle);
            return res != nullptr ? res->lastKnownState : rhi::ResourceState::Undefined;
        }

        [[nodiscard]] const std::vector<i32>& executionOrder() const noexcept { return m_executionOrder; }
        [[nodiscard]] const std::vector<RenderGraphPass*>& passes() const noexcept { return m_passes; }
        [[nodiscard]] const std::vector<RenderGraphResource*>& resources() const noexcept { return m_resources; }

    private:
        struct DeferredDeletion
        {
            rhi::Texture* texture = nullptr;
            rhi::TextureView* view = nullptr;
            rhi::TextureView* view2 = nullptr;
            rhi::Buffer* buffer = nullptr;

            void execute(rhi::Device* device)
            {
                if (device == nullptr) { return; }
                if (view2 != nullptr) { device->destroyTextureView(view2); }
                if (view != nullptr) { device->destroyTextureView(view); }
                if (texture != nullptr) { device->destroyTexture(texture); }
                if (buffer != nullptr) { device->destroyBuffer(buffer); }
            }
        };

        [[nodiscard]] std::span<RenderGraphResource* const> resourceSpan() const
        {
            return std::span<RenderGraphResource* const>(m_resources.data(), m_resources.size());
        }

        [[nodiscard]] std::vector<DeferredDeletion>& deferredSlot()
        {
            const i32 slot = m_frameIndex % static_cast<i32>(m_deferredDeletions.size());
            return m_deferredDeletions[static_cast<usize>(slot)];
        }

        void flushDeferred(i32 frameIndex)
        {
            const i32 slot = frameIndex % static_cast<i32>(m_deferredDeletions.size());
            std::vector<DeferredDeletion>& deletions = m_deferredDeletions[static_cast<usize>(slot)];
            for (DeferredDeletion& d : deletions) { d.execute(m_device); }
            deletions.clear();
        }

        // Validate a handle (bounds + generation); null on mismatch.
        [[nodiscard]] RenderGraphResource* resolveChecked(RGHandle handle)
        {
            if (!handle.isValid() || handle.index >= m_resources.size()) { return nullptr; }
            RenderGraphResource* res = m_resources[handle.index];
            if (res == nullptr || res->generation != handle.generation) { return nullptr; }
            return res;
        }
        // Validate a handle (bounds only; ignores generation) - for mutators.
        [[nodiscard]] RenderGraphResource* resolve(RGHandle handle)
        {
            if (!handle.isValid() || handle.index >= m_resources.size()) { return nullptr; }
            return m_resources[handle.index];
        }

        RGHandle addResource(RenderGraphResource* res)
        {
            if (!m_freeResourceSlots.empty())
            {
                const i32 idx = m_freeResourceSlots.back();
                m_freeResourceSlots.pop_back();
                m_resources[static_cast<usize>(idx)] = res;
                return RGHandle{ static_cast<u32>(idx), res->generation };
            }
            const u32 idx = static_cast<u32>(m_resources.size());
            m_resources.push_back(res);
            return RGHandle{ idx, res->generation };
        }

        template <typename Setup>
        PassHandle addPassOfType(std::u8string_view name, RGPassType type, Setup&& setup)
        {
            RenderGraphPass* pass = new RenderGraphPass(name, type);
            PassBuilder builder(*pass);
            setup(builder);
            const u32 idx = static_cast<u32>(m_passes.size());
            m_passes.push_back(pass);
            return PassHandle{ idx };
        }

        void clearPasses()
        {
            for (RenderGraphPass* p : m_passes) { delete (p); }
            m_passes.clear();
            m_executionOrder.clear();
        }

        void recycleNonPersistent()
        {
            for (usize i = 0; i < m_resources.size(); ++i)
            {
                RenderGraphResource* res = m_resources[i];
                if (res == nullptr) { continue; }
                if (res->lifetime != RGResourceLifetime::Persistent)
                {
                    m_freeResourceSlots.push_back(static_cast<i32>(i));
                    delete (res);
                    m_resources[i] = nullptr;
                }
                else
                {
                    res->resetTracking();
                }
            }
        }

        // === compilation pipeline ===
        void buildResourceReferences()
        {
            for (usize passIdx = 0; passIdx < m_passes.size(); ++passIdx)
            {
                RenderGraphPass* pass = m_passes[passIdx];
                const PassHandle passHandle{ static_cast<u32>(passIdx) };

                for (const RGResourceAccess& access : pass->accesses)
                {
                    if (!access.handle.isValid() || access.handle.index >= m_resources.size()) { continue; }
                    RenderGraphResource* res = m_resources[access.handle.index];
                    if (res == nullptr) { continue; }

                    ++res->refCount;
                    if (res->firstUsePass < 0 || static_cast<i32>(passIdx) < res->firstUsePass) { res->firstUsePass = static_cast<i32>(passIdx); }
                    if (static_cast<i32>(passIdx) > res->lastUsePass) { res->lastUsePass = static_cast<i32>(passIdx); }
                    if (access.isWrite()) { res->firstWriter = passHandle; }
                    if (access.isRead()) { res->lastReader = passHandle; }
                }
            }
        }

        void cullPasses()
        {
            for (RenderGraphPass* pass : m_passes) { pass->isCulled = true; }
            for (RenderGraphPass* pass : m_passes) { if (pass->shouldSurviveCulling()) { pass->isCulled = false; } }

            // Passes writing imported resources with a final state stay alive.
            for (RenderGraphPass* pass : m_passes)
            {
                if (!pass->isCulled) { continue; }
                std::vector<RGResourceAccess> outputs;
                pass->getOutputs(outputs);
                for (const RGResourceAccess& output : outputs)
                {
                    if (output.handle.isValid() && output.handle.index < m_resources.size())
                    {
                        RenderGraphResource* res = m_resources[output.handle.index];
                        if (res != nullptr && res->finalState.has_value()) { pass->isCulled = false; break; }
                    }
                }
            }

            // Backward propagation: a live pass keeps its overlapping upstream writers alive.
            bool changed = true;
            while (changed)
            {
                changed = false;
                for (RenderGraphPass* pass : m_passes)
                {
                    if (pass->isCulled) { continue; }
                    std::vector<RGResourceAccess> inputs;
                    pass->getInputs(inputs);

                    for (const RGResourceAccess& input : inputs)
                    {
                        if (!input.handle.isValid() || input.handle.index >= m_resources.size()) { continue; }
                        for (usize i = m_passes.size(); i-- > 0;)
                        {
                            RenderGraphPass* candidate = m_passes[i];
                            if (!candidate->isCulled) { continue; }
                            std::vector<RGResourceAccess> candidateOutputs;
                            candidate->getOutputs(candidateOutputs);
                            for (const RGResourceAccess& output : candidateOutputs)
                            {
                                if (output.handle == input.handle
                                    && (input.subresource.isAll() || output.subresource.isAll()
                                        || input.subresource.overlaps(output.subresource)))
                                {
                                    candidate->isCulled = false;
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        void buildDependencies()
        {
            for (usize passIdx = 0; passIdx < m_passes.size(); ++passIdx)
            {
                RenderGraphPass* pass = m_passes[passIdx];
                if (pass->isCulled) { continue; }

                std::vector<RGResourceAccess> readAccesses;
                pass->getInputs(readAccesses);

                for (const RGResourceAccess& readAccess : readAccesses)
                {
                    if (!readAccess.handle.isValid() || readAccess.handle.index >= m_resources.size()) { continue; }
                    RenderGraphResource* res = m_resources[readAccess.handle.index];
                    const u32 totalMips = res != nullptr ? res->totalMipLevels() : 1u;
                    const u32 totalLayers = res != nullptr ? res->totalArrayLayers() : 1u;

                    for (usize j = passIdx; j-- > 0;)
                    {
                        RenderGraphPass* writer = m_passes[j];
                        if (writer->isCulled) { continue; }
                        std::vector<RGResourceAccess> writerOutputs;
                        writer->getOutputs(writerOutputs);

                        bool overlaps = false;
                        for (const RGResourceAccess& writerAccess : writerOutputs)
                        {
                            if (writerAccess.handle == readAccess.handle
                                && (readAccess.subresource.isAll() || writerAccess.subresource.isAll()
                                    || readAccess.subresource.overlaps(writerAccess.subresource, totalMips, totalLayers)))
                            {
                                overlaps = true;
                                break;
                            }
                        }
                        if (overlaps) { addDependencyIfNew(*pass, PassHandle{ static_cast<u32>(j) }); break; }
                    }
                }
            }
        }

        static void addDependencyIfNew(RenderGraphPass& pass, PassHandle dep)
        {
            for (PassHandle existing : pass.dependencies) { if (existing == dep) { return; } }
            pass.dependencies.push_back(dep);
        }

        [[nodiscard]] Status topologicalSort()
        {
            m_executionOrder.clear();
            const usize passCount = m_passes.size();

            std::vector<i32> inDegree;
            inDegree.resize(passCount);
            std::vector<std::vector<i32>> adjacency;
            for (usize i = 0; i < passCount; ++i) { adjacency.push_back(std::vector<i32>{}); }

            for (usize i = 0; i < passCount; ++i)
            {
                RenderGraphPass* pass = m_passes[i];
                if (pass->isCulled) { continue; }
                for (PassHandle dep : pass->dependencies)
                {
                    if (dep.isValid() && dep.index < passCount)
                    {
                        adjacency[dep.index].push_back(static_cast<i32>(i));
                        ++inDegree[i];
                    }
                }
            }

            std::vector<i32> queue;
            for (usize i = 0; i < passCount; ++i)
            {
                if (!m_passes[i]->isCulled && inDegree[i] == 0) { queue.push_back(static_cast<i32>(i)); }
            }

            while (!queue.empty())
            {
                const i32 node = queue[0];
                queue.erase(queue.begin());
                m_executionOrder.push_back(node);
                m_passes[static_cast<usize>(node)]->executionOrder = static_cast<i32>(m_executionOrder.size()) - 1;

                for (i32 neighbor : adjacency[static_cast<usize>(node)])
                {
                    if (--inDegree[static_cast<usize>(neighbor)] == 0) { queue.push_back(neighbor); }
                }
            }

            usize nonCulled = 0;
            for (RenderGraphPass* p : m_passes) { if (!p->isCulled) { ++nonCulled; } }
            return m_executionOrder.size() == nonCulled ? Status{} : Status{ ErrorCode::Unknown }; // cycle
        }

        void allocateTransientResources()
        {
            for (RenderGraphResource* res : m_resources)
            {
                if (res == nullptr || res->lifetime != RGResourceLifetime::Transient || res->refCount == 0) { continue; }

                if (res->resourceType == RGResourceType::Texture && m_device != nullptr)
                {
                    rhi::TextureDesc rhiDesc = res->textureDesc.toTextureDesc(res->name);
                    rhi::Texture* tex = nullptr;
                    rhi::TextureView* view = nullptr;
                    u64 pooledGen = 0;
                    if (m_texturePool.get() != nullptr && m_texturePool->tryAcquire(rhiDesc, tex, view, pooledGen))
                    {
                        res->texture = tex;
                        res->textureView = view;
                        res->textureGeneration = pooledGen;   // reused physical texture keeps its id
                        if (rhi::isDepthFormat(res->textureDesc.format) && rhi::hasStencil(res->textureDesc.format))
                        {
                            rhi::TextureViewDesc depthDesc{};
                            depthDesc.aspect = rhi::TextureAspect::DepthOnly;
                            depthDesc.label = u8"RGDepthOnlyView";
                            rhi::TextureView* depthOnly = nullptr;
                            if (m_device->createTextureView(tex, depthDesc, depthOnly).isOk()) { res->depthOnlyView = depthOnly; }
                        }
                    }
                    else
                    {
                        (void)res->allocateTexture(*m_device);
                        res->textureGeneration = ++m_nextTransientGeneration;   // freshly created -> new id
                    }
                }
                else if (res->resourceType == RGResourceType::Buffer && m_device != nullptr)
                {
                    (void)res->allocateBuffer(*m_device);
                }
            }
        }

        void returnTransientResources()
        {
            std::vector<DeferredDeletion>& deletions = deferredSlot();
            for (RenderGraphResource* res : m_resources)
            {
                if (res == nullptr || res->lifetime != RGResourceLifetime::Transient) { continue; }

                if (res->resourceType == RGResourceType::Texture && res->texture != nullptr)
                {
                    if (m_texturePool.get() != nullptr)
                    {
                        const rhi::TextureDesc rhiDesc = res->textureDesc.toTextureDesc(res->name);
                        m_texturePool->returnToPool(rhiDesc, res->texture, res->textureView, res->textureGeneration);
                        if (res->depthOnlyView != nullptr) { DeferredDeletion d{}; d.view = res->depthOnlyView; deletions.push_back(d); }
                    }
                    else
                    {
                        DeferredDeletion d{}; d.texture = res->texture; d.view = res->textureView; d.view2 = res->depthOnlyView;
                        deletions.push_back(d);
                    }
                    res->texture = nullptr;
                    res->textureView = nullptr;
                    res->depthOnlyView = nullptr;
                }
                else if (res->resourceType == RGResourceType::Buffer && res->buffer != nullptr)
                {
                    DeferredDeletion d{}; d.buffer = res->buffer; deletions.push_back(d);
                    res->buffer = nullptr;
                }
            }
        }

        // === pass execution ===
        rhi::TextureView* createSubresourceView(RGHandle handle, RGSubresourceRange subresource)
        {
            if (m_device == nullptr) { return nullptr; }
            rhi::Texture* texture = getTexture(handle);
            if (texture == nullptr) { return nullptr; }

            rhi::TextureViewDesc viewDesc{};
            viewDesc.baseMipLevel = subresource.baseMipLevel;
            viewDesc.mipLevelCount = subresource.mipLevelCount == 0 ? 1u : subresource.mipLevelCount;
            viewDesc.baseArrayLayer = subresource.baseArrayLayer;
            viewDesc.arrayLayerCount = subresource.arrayLayerCount == 0 ? 1u : subresource.arrayLayerCount;
            viewDesc.dimension = viewDesc.arrayLayerCount == 1 ? rhi::TextureViewDimension::Texture2D
                                                              : rhi::TextureViewDimension::Texture2DArray;
            rhi::TextureView* view = nullptr;
            if (m_device->createTextureView(texture, viewDesc, view).isOk())
            {
                m_subresourceViews.push_back(view);
                return view;
            }
            return nullptr;
        }

        // The full-target render area for a pass, from an attachment's resolved dimensions (the
        // transient desc, else the backing texture). Used to set the parent pass's viewport +
        // scissor before ExecuteBundles: render bundles INHERIT viewport/scissor from the parent
        // pass (WebGPU + DX12 bundles cannot set them), so the parent must. Returns false if no
        // attachment yields dimensions.
        [[nodiscard]] bool passRenderArea(RenderGraphPass& pass, u32& outW, u32& outH)
        {
            auto fromHandle = [&](RGHandle h, u32& w, u32& h2) -> bool {
                if (RenderGraphResource* res = resolve(h)) {
                    if (res->textureDesc.width > 0 && res->textureDesc.height > 0) {
                        w = res->textureDesc.width; h2 = res->textureDesc.height; return true;
                    }
                }
                if (rhi::TextureView* v = getTextureView(h)) {
                    if (v->texture != nullptr && v->texture->desc.width > 0 && v->texture->desc.height > 0) {
                        w = v->texture->desc.width; h2 = v->texture->desc.height; return true;
                    }
                }
                return false;
            };
            for (const RGColorTarget& ct : pass.colorTargets) { if (fromHandle(ct.handle, outW, outH)) { return true; } }
            if (pass.depthTarget.has_value()) { if (fromHandle(pass.depthTarget.value().handle, outW, outH)) { return true; } }
            return false;
        }

        void executeRenderPass(RenderGraphPass& pass, rhi::CommandEncoder& encoder)
        {
            const bool hasBundles = static_cast<bool>(pass.bundleCallback);
            if (!static_cast<bool>(pass.executeCallback) && !hasBundles) { return; }

            // A bundle pass records its bundles NOW (encoder in recording state, before the pass
            // begins); the graph then begins with secondary contents + replays them. Done before
            // building the pass desc so the encoder is still recording.
            std::vector<rhi::RenderBundle*> bundles;
            if (hasBundles) { pass.bundleCallback(encoder, bundles); }

            rhi::RenderPassDesc rpDesc{};
            rpDesc.label = pass.name;
            if (hasBundles) { rpDesc.contents = rhi::RenderPassContents::SecondaryCommandBuffers; }

            for (usize i = 0; i < pass.colorTargets.size(); ++i)
            {
                const RGColorTarget& ct = pass.colorTargets[i];
                rhi::TextureView* view = getTextureView(ct.handle);
                if (view == nullptr) { continue; }
                if (!ct.subresource.isAll())
                {
                    if (rhi::TextureView* subView = createSubresourceView(ct.handle, ct.subresource)) { view = subView; }
                }
                rhi::ColorAttachment attachment{};
                attachment.view = view;
                attachment.loadOp = ct.loadOp;
                attachment.storeOp = ct.storeOp;
                attachment.clearValue = ct.clearValue;
                rpDesc.colorAttachments.push_back(attachment);
            }

            if (pass.depthTarget.has_value())
            {
                const RGDepthTarget& dt = pass.depthTarget.value();
                rhi::TextureView* view = getTextureView(dt.handle);
                if (view != nullptr)
                {
                    if (!dt.subresource.isAll())
                    {
                        if (rhi::TextureView* subView = createSubresourceView(dt.handle, dt.subresource)) { view = subView; }
                    }
                    rhi::DepthStencilAttachment dsa{};
                    dsa.view = view;
                    dsa.depthLoadOp = dt.depthLoadOp;
                    dsa.depthStoreOp = dt.depthStoreOp;
                    dsa.depthClearValue = dt.depthClearValue;
                    dsa.depthReadOnly = dt.readOnly;
                    dsa.stencilLoadOp = dt.stencilLoadOp;
                    dsa.stencilStoreOp = dt.stencilStoreOp;
                    dsa.stencilClearValue = dt.stencilClearValue;
                    rpDesc.depthStencilAttachment = dsa;
                }
            }

            rhi::RenderPassEncoder* rp = encoder.beginRenderPass(rpDesc);
            // Viewport/scissor: a per-pass override (split-screen sub-rect) if set, else the full
            // attachment. Set here for bundle passes (bundles inherit it from the parent - WebGPU/
            // DX12 can't set it inside a bundle); a plain execute callback may also rely on it.
            i32 vpX = 0, vpY = 0; u32 vpW = 0, vpH = 0;
            if (pass.hasViewport) { vpX = pass.viewportX; vpY = pass.viewportY; vpW = pass.viewportW; vpH = pass.viewportH; }
            else { (void)passRenderArea(pass, vpW, vpH); }
            if (vpW > 0 && vpH > 0) {
                rp->setViewport(static_cast<f32>(vpX), static_cast<f32>(vpY), static_cast<f32>(vpW), static_cast<f32>(vpH));
                rp->setScissor(vpX, vpY, vpW, vpH);
            }
            if (hasBundles) {
                if (!bundles.empty()) {
                    rp->executeBundles(std::span<rhi::RenderBundle* const>{ bundles.data(), bundles.size() });
                }
            } else {
                pass.executeCallback(*rp);
            }
            rp->end();
        }

        void executeComputePass(RenderGraphPass& pass, rhi::CommandEncoder& encoder)
        {
            if (!static_cast<bool>(pass.computeCallback)) { return; }
            rhi::ComputePassEncoder* cp = encoder.beginComputePass(pass.name);
            pass.computeCallback(*cp);
            cp->end();
        }

        void executeCopyPass(RenderGraphPass& pass, rhi::CommandEncoder& encoder)
        {
            if (!static_cast<bool>(pass.copyCallback)) { return; }
            pass.copyCallback(encoder);
        }

        rhi::Device* m_device;
        RenderGraphConfig m_config;
        std::vector<RenderGraphResource*> m_resources;
        std::vector<i32> m_freeResourceSlots;
        std::vector<RenderGraphPass*> m_passes;
        std::vector<i32> m_executionOrder;
        bool m_isCompiled = false;
        std::unique_ptr<GraphProfiler> m_gpuProfiler;     // optional per-pass GPU timing
        i32 m_lastProfiledPassCount = 0;
        struct PassCpu { std::u8string_view name; u64 ticks = 0; };   // per-pass CPU record time (name -> pass->name, valid pre-Reset)
        std::vector<PassCpu> m_passCpu;
        BarrierSolver m_barrierSolver;
        std::unique_ptr<TransientTexturePool> m_texturePool;
        std::vector<std::vector<DeferredDeletion>> m_deferredDeletions;
        std::vector<rhi::TextureView*> m_subresourceViews;
        i32 m_frameIndex = 0;
        u32 m_outputWidth = 1920;
        u32 m_outputHeight = 1080;
        u64 m_nextTransientGeneration = 0;   // monotonic id stamped on each freshly created transient texture
    };
}
