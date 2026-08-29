// Draconic::RenderGraph - :callbacks partition
//
// Pass execution callbacks. Sedulous uses Beef delegates; here they are Core
// Function objects over the RHI encoder a pass records into.

module;

#include <vector>
#include <functional>

export module rendergraph:callbacks;

import core;
import rhi;

using namespace draco;

export namespace draco::rendergraph
{
    namespace rhi = draco::rhi;

    using RenderPassExecuteCallback  = std::function<void(rhi::RenderPassEncoder&)>;
    using ComputePassExecuteCallback = std::function<void(rhi::ComputePassEncoder&)>;
    using CopyPassExecuteCallback    = std::function<void(rhi::CommandEncoder&)>;

    // A render pass whose body is supplied by render bundles (recorded off-thread). Called with
    // the command encoder in the RECORDING state (before the render pass begins, so bundles can
    // be created) and an out-list to fill with the bundles to replay; the graph then begins the
    // pass with secondary-command-buffer contents and ExecuteBundles them in order. This is what
    // lets parallel command recording run inside the frame graph.
    using RenderBundlePassCallback   = std::function<void(rhi::CommandEncoder&, std::vector<rhi::RenderBundle*>&)>;
}
