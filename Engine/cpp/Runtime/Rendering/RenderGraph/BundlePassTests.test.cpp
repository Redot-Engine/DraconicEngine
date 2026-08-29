// Render-bundle pass: a render pass whose body is supplied by render bundles (the rendergraph
// extension that lets parallel command recording run inside the frame graph). Driven on the
// Null RHI so Execute actually runs the pass.
#include <doctest_with_main.h>

import core;
import rhi;
import rhi.null;
import rendergraph;

using namespace draco;
using namespace draco::rendergraph;
namespace rhi = draco::rhi;

namespace {
struct GraphHarness {
    rhi::null::NullDevice device;
    rhi::Texture*       tex   = nullptr;
    rhi::TextureView*   view  = nullptr;
    rhi::CommandPool*   pool  = nullptr;
    rhi::CommandEncoder* enc  = nullptr;

    bool init() {
        if (!device.createTexture(rhi::TextureDesc::renderTarget(rhi::TextureFormat::BGRA8Unorm, 64, 64), tex).isOk()) { return false; }
        rhi::TextureViewDesc vd{}; vd.format = rhi::TextureFormat::BGRA8Unorm;
        if (!device.createTextureView(tex, vd, view).isOk()) { return false; }
        if (!device.createCommandPool(rhi::QueueType::Graphics, pool).isOk()) { return false; }
        return pool->createEncoder(enc).isOk();
    }
    ~GraphHarness() {
        if (pool) { device.destroyCommandPool(pool); }
        if (view) { device.destroyTextureView(view); }
        if (tex)  { device.destroyTexture(tex); }
    }
};
}

TEST_CASE("rg.bundle: a bundle pass records bundles before the pass + executes them")
{
    GraphHarness h;
    REQUIRE(h.init());

    RenderGraph graph(&h.device);
    graph.setOutputSize(64, 64);
    graph.beginFrame(0);
    const RGHandle color = graph.importTarget(u8"BB", h.tex, h.view, rhi::ResourceState::Present);

    bool ran = false;
    usize bundleCount = 0;
    graph.addRenderPass(u8"BundlePass", [&](PassBuilder& b) {
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
        b.setBundleExecute([&](rhi::CommandEncoder& enc, std::vector<rhi::RenderBundle*>& out) {
            ran = true;   // called with the encoder in the recording state, before the pass begins
            rhi::RenderBundleDesc bd{};
            bd.colorFormats[0] = rhi::TextureFormat::BGRA8Unorm; bd.colorFormatCount = 1;
            bd.width = 64; bd.height = 64;
            if (rhi::RenderBundleEncoder* be = enc.createRenderBundleEncoder(bd)) { out.push_back(be->finish()); }
            bundleCount = out.size();
        });
    });

    CHECK(graph.execute(h.enc).isOk());
    CHECK(ran);                  // the bundle callback ran during execution
    CHECK(bundleCount == 1u);    // it produced a bundle for the graph to ExecuteBundles
}

TEST_CASE("rg.bundle: a bundle pass that produces no bundles still runs (clears only)")
{
    GraphHarness h;
    REQUIRE(h.init());

    RenderGraph graph(&h.device);
    graph.setOutputSize(64, 64);
    graph.beginFrame(0);
    const RGHandle color = graph.importTarget(u8"BB", h.tex, h.view, rhi::ResourceState::Present);

    bool ran = false;
    graph.addRenderPass(u8"EmptyBundlePass", [&](PassBuilder& b) {
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
        b.setBundleExecute([&](rhi::CommandEncoder&, std::vector<rhi::RenderBundle*>&) { ran = true; });
    });

    CHECK(graph.execute(h.enc).isOk());
    CHECK(ran);
}
