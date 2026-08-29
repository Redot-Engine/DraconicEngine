// Draconic::RenderGraph - :pass partition
//
// A single pass: its declared resource accesses, attachments, dependencies, and
// typed execute callback. GetInputs/GetOutputs fold attachment load/store ops
// into the read/write access set the compiler reasons about. Ported from
// Sedulous.RenderGraph (RenderGraphPass.bf).

module;

#include <vector>
#include <optional>
#include <functional>
#include <string>
#include <string_view>

export module rendergraph:pass;

import core;
import rhi;
import :types;
import :descriptors;
import :callbacks;

using namespace draco;

export namespace draco::rendergraph
{
    namespace rhi = draco::rhi;

    class RenderGraphPass
    {
    public:
        RenderGraphPass(std::u8string_view passName, RGPassType passType)
            : name(passName), type(passType) {}

        // Explicitly defaulted so the (move-only, due to Function members) special
        // members are synthesized in this module and usable by importers - GCC's
        // module support otherwise reports the implicit destructor as deleted.
        ~RenderGraphPass() = default;
        RenderGraphPass(RenderGraphPass&&) = default;
        RenderGraphPass& operator=(RenderGraphPass&&) = default;
        RenderGraphPass(const RenderGraphPass&) = delete;
        RenderGraphPass& operator=(const RenderGraphPass&) = delete;

        // Resource handles this pass reads (declared accesses + Load attachments).
        void getInputs(std::vector<RGResourceAccess>& out) const
        {
            for (const RGResourceAccess& access : accesses)
            {
                if (access.isRead()) { out.push_back(access); }
            }
            for (const RGColorTarget& ct : colorTargets)
            {
                if (ct.loadOp == rhi::LoadOp::Load)
                {
                    out.push_back(RGResourceAccess{ ct.handle, RGAccessType::ReadTexture, ct.subresource });
                }
            }
            if (depthTarget.has_value())
            {
                const RGDepthTarget& dt = depthTarget.value();
                if (dt.depthLoadOp == rhi::LoadOp::Load || dt.readOnly)
                {
                    out.push_back(RGResourceAccess{ dt.handle, RGAccessType::ReadDepthStencil, dt.subresource });
                }
            }
        }

        // Resource handles this pass writes (declared accesses + Store attachments).
        void getOutputs(std::vector<RGResourceAccess>& out) const
        {
            for (const RGResourceAccess& access : accesses)
            {
                if (access.isWrite()) { out.push_back(access); }
            }
            for (const RGColorTarget& ct : colorTargets)
            {
                if (ct.storeOp == rhi::StoreOp::Store)
                {
                    out.push_back(RGResourceAccess{ ct.handle, RGAccessType::WriteColorTarget, ct.subresource });
                }
            }
            if (depthTarget.has_value())
            {
                const RGDepthTarget& dt = depthTarget.value();
                if (dt.depthStoreOp == rhi::StoreOp::Store && !dt.readOnly)
                {
                    out.push_back(RGResourceAccess{ dt.handle, RGAccessType::WriteDepthTarget, dt.subresource });
                }
            }
        }

        [[nodiscard]] bool shouldSurviveCulling() const noexcept { return neverCull || hasSideEffects; }

        // --- identity ---
        std::u8string name;
        RGPassType type;
        rhi::QueueType queueType = rhi::QueueType::Graphics;

        // --- declared work ---
        std::vector<RGResourceAccess> accesses;
        std::vector<RGColorTarget> colorTargets;
        std::optional<RGDepthTarget> depthTarget;
        std::vector<PassHandle> dependencies;

        // --- optional per-pass viewport/scissor override (else the full attachment is used) ---
        bool hasViewport = false;
        i32  viewportX = 0, viewportY = 0;
        u32  viewportW = 0, viewportH = 0;

        // --- compile flags ---
        bool isCulled = false;
        bool neverCull = false;
        bool hasSideEffects = false;
        std::function<bool()> condition;          // optional runtime skip condition
        i32 executionOrder = -1;             // assigned during topological sort

        // --- typed execute callbacks (one is set per pass type) ---
        RenderPassExecuteCallback executeCallback;
        RenderBundlePassCallback bundleCallback;   // render pass whose body is executed bundles
        ComputePassExecuteCallback computeCallback;
        CopyPassExecuteCallback copyCallback;
    };
}
