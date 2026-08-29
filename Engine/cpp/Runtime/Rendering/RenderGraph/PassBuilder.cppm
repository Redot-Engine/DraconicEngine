// Draconic::RenderGraph - :pass_builder partition
//
// Fluent builder handed to a pass's setup callback to declare reads/writes,
// attachments, dependencies, flags, and the execute callback. Ported from
// Sedulous.RenderGraph (PassBuilder.bf). Methods return *this for chaining.

module;

#include <functional>
#include <utility>

export module rendergraph:pass_builder;

import core;
import rhi;
import :types;
import :descriptors;
import :callbacks;
import :pass;

using namespace draco;

export namespace draco::rendergraph
{
    namespace rhi = draco::rhi;

    class PassBuilder
    {
    public:
        explicit PassBuilder(RenderGraphPass& pass) noexcept : m_pass(&pass) {}

        // --- texture / buffer reads ---
        PassBuilder& readTexture(RGHandle handle, RGSubresourceRange subresource = {})
        {
            m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::ReadTexture, subresource });
            return *this;
        }

        // Sample a DEPTH texture in this pass's shader (e.g. a shadow map). Like ReadTexture - a read
        // dependency + barrier, NOT a depth attachment (unlike ReadDepth) - but transitions to
        // DepthStencilRead (DEPTH_STENCIL_READ_ONLY_OPTIMAL), the layout a depth sampler expects.
        PassBuilder& sampleDepth(RGHandle handle, RGSubresourceRange subresource = {})
        {
            m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::SampleDepthStencil, subresource });
            return *this;
        }

        PassBuilder& readDepth(RGHandle handle, RGSubresourceRange subresource = {})
        {
            RGDepthTarget dt{};
            dt.handle = handle;
            dt.depthLoadOp = rhi::LoadOp::Load;
            dt.depthStoreOp = rhi::StoreOp::Store;
            dt.readOnly = true;
            dt.subresource = subresource;
            m_pass->depthTarget = dt;
            m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::ReadDepthStencil, subresource });
            return *this;
        }

        PassBuilder& readBuffer(RGHandle handle)
        {
            m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::ReadBuffer, {} });
            return *this;
        }

        // --- render targets ---
        PassBuilder& setColorTarget(i32 slot, RGHandle handle,
                                    rhi::LoadOp loadOp = rhi::LoadOp::Clear,
                                    rhi::StoreOp storeOp = rhi::StoreOp::Store,
                                    rhi::ClearColor clearValue = rhi::ClearColor::black(),
                                    RGSubresourceRange subresource = {})
        {
            RGColorTarget target{};
            target.handle = handle;
            target.loadOp = loadOp;
            target.storeOp = storeOp;
            target.clearValue = clearValue;
            target.subresource = subresource;

            while (static_cast<i32>(m_pass->colorTargets.size()) <= slot) { m_pass->colorTargets.push_back(RGColorTarget{}); }
            m_pass->colorTargets[static_cast<usize>(slot)] = target;

            if (loadOp == rhi::LoadOp::Load && storeOp == rhi::StoreOp::Store)
            {
                m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::ReadWriteColorTarget, subresource });
            }
            else if (storeOp == rhi::StoreOp::Store)
            {
                m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::WriteColorTarget, subresource });
            }
            return *this;
        }

        PassBuilder& setDepthTarget(RGHandle handle,
                                    rhi::LoadOp loadOp = rhi::LoadOp::Clear,
                                    rhi::StoreOp storeOp = rhi::StoreOp::Store,
                                    f32 clearDepth = 1.0f,
                                    RGSubresourceRange subresource = {})
        {
            RGDepthTarget dt{};
            dt.handle = handle;
            dt.depthLoadOp = loadOp;
            dt.depthStoreOp = storeOp;
            dt.depthClearValue = clearDepth;
            dt.readOnly = false;
            dt.stencilLoadOp = rhi::LoadOp::DontCare;
            dt.stencilStoreOp = rhi::StoreOp::DontCare;
            dt.subresource = subresource;
            m_pass->depthTarget = dt;

            if (loadOp == rhi::LoadOp::Load && storeOp == rhi::StoreOp::Store)
            {
                m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::ReadWriteDepthTarget, subresource });
            }
            else if (storeOp == rhi::StoreOp::Store)
            {
                m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::WriteDepthTarget, subresource });
            }
            return *this;
        }

        // Read-only depth (depth test, no write): transitions to DepthStencilRead.
        PassBuilder& setReadOnlyDepthTarget(RGHandle handle, RGSubresourceRange subresource = {})
        {
            RGDepthTarget dt{};
            dt.handle = handle;
            dt.depthLoadOp = rhi::LoadOp::Load;
            dt.depthStoreOp = rhi::StoreOp::Store;
            dt.depthClearValue = 1.0f;
            dt.readOnly = true;
            dt.subresource = subresource;
            m_pass->depthTarget = dt;
            m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::ReadDepthStencil, subresource });
            return *this;
        }

        // --- storage (UAV) ---
        PassBuilder& writeStorage(RGHandle handle, RGSubresourceRange subresource = {})
        {
            m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::WriteStorage, subresource });
            return *this;
        }
        PassBuilder& readWriteStorage(RGHandle handle, RGSubresourceRange subresource = {})
        {
            m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::ReadWriteStorage, subresource });
            return *this;
        }

        // --- copy ---
        PassBuilder& copySrc(RGHandle handle)
        {
            m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::ReadCopySrc, {} });
            return *this;
        }
        PassBuilder& copyDst(RGHandle handle)
        {
            m_pass->accesses.push_back(RGResourceAccess{ handle, RGAccessType::WriteCopyDst, {} });
            return *this;
        }

        // --- dependencies / flags ---
        PassBuilder& dependsOn(PassHandle pass) { m_pass->dependencies.push_back(pass); return *this; }
        // Override the pass's viewport + scissor (a sub-rect of the attachment, e.g. split-screen).
        // Without it the pass covers the full attachment.
        PassBuilder& setViewport(i32 x, i32 y, u32 w, u32 h) {
            m_pass->hasViewport = true; m_pass->viewportX = x; m_pass->viewportY = y;
            m_pass->viewportW = w; m_pass->viewportH = h; return *this;
        }
        PassBuilder& neverCull() { m_pass->neverCull = true; return *this; }
        PassBuilder& hasSideEffects() { m_pass->hasSideEffects = true; return *this; }
        PassBuilder& enableIf(std::function<bool()> condition) { m_pass->condition = std::move(condition); return *this; }

        // --- execute callbacks ---
        PassBuilder& setExecute(RenderPassExecuteCallback callback) { m_pass->executeCallback = std::move(callback); return *this; }
        // A render pass whose body is supplied by render bundles (parallel command recording).
        // The graph begins the pass with secondary-command-buffer contents + ExecuteBundles them.
        PassBuilder& setBundleExecute(RenderBundlePassCallback callback) { m_pass->bundleCallback = std::move(callback); return *this; }
        PassBuilder& setComputeExecute(ComputePassExecuteCallback callback) { m_pass->computeCallback = std::move(callback); return *this; }
        PassBuilder& setCopyExecute(CopyPassExecuteCallback callback) { m_pass->copyCallback = std::move(callback); return *this; }

    private:
        RenderGraphPass* m_pass;
    };
}
