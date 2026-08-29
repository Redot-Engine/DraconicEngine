// Transient texture generation: each transient carries a stable id for its backing physical texture,
// surfaced via GetTextureGeneration. A bind-group cache over a transient's view keys on this (not the
// raw pointer) so a reused-address view can't alias a stale, destroyed texture across a resize.
// Driven on the Null RHI so Execute actually allocates/returns the transient.
#include <doctest_with_main.h>

import core;
import rhi;
import rhi.null;
import rendergraph;

using namespace draco;
using namespace draco::rendergraph;
namespace rhi = draco::rhi;

namespace {
struct Harness {
    rhi::null::NullDevice device;
    rhi::Texture*        bb     = nullptr;
    rhi::TextureView*    bbView = nullptr;
    rhi::CommandPool*    pool   = nullptr;
    rhi::CommandEncoder* enc    = nullptr;

    bool init() {
        if (!device.createTexture(rhi::TextureDesc::renderTarget(rhi::TextureFormat::BGRA8Unorm, 64, 64), bb).isOk()) { return false; }
        rhi::TextureViewDesc vd{}; vd.format = rhi::TextureFormat::BGRA8Unorm;
        if (!device.createTextureView(bb, vd, bbView).isOk()) { return false; }
        if (!device.createCommandPool(rhi::QueueType::Graphics, pool).isOk()) { return false; }
        return pool->createEncoder(enc).isOk();
    }
    ~Harness() {
        if (pool)   { device.destroyCommandPool(pool); }
        if (bbView) { device.destroyTextureView(bbView); }
        if (bb)     { device.destroyTexture(bb); }
    }
};

// One frame: a transient HDR target written by one pass and read by a backbuffer pass (so it isn't
// culled). Captures the transient's generation + view inside the writing pass's execute.
void runFrame(RenderGraph& graph, Harness& h, i32 frameIndex, u64& outGen, rhi::TextureView*& outView) {
    graph.setOutputSize(64, 64);
    graph.beginFrame(frameIndex);
    const RGHandle bbH = graph.importTarget(u8"BB", h.bb, h.bbView, rhi::ResourceState::Present);
    const RGHandle hdr = graph.createTransient(u8"HDR", RGTextureDesc(rhi::TextureFormat::RGBA16Float, 64, 64));
    graph.addRenderPass(u8"WriteHDR", [&](PassBuilder& b) {
        b.setColorTarget(0, hdr, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
        b.setExecute([&](rhi::RenderPassEncoder&) {
            outGen  = graph.getTextureGeneration(hdr);
            outView = graph.getTextureView(hdr);
        });
    });
    graph.addRenderPass(u8"ReadHDR", [&](PassBuilder& b) {
        b.setColorTarget(0, bbH, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.readTexture(hdr);
        b.neverCull();
        b.setExecute([](rhi::RenderPassEncoder&) {});
    });
    CHECK(graph.execute(h.enc).isOk());
    graph.endFrame();
}
}

TEST_CASE("rg.transient: generation is non-zero and stable across pool reuse")
{
    Harness h; REQUIRE(h.init());
    RenderGraph graph(&h.device);

    u64 gen0 = 0, gen1 = 0;
    rhi::TextureView* v0 = nullptr; rhi::TextureView* v1 = nullptr;
    runFrame(graph, h, 0, gen0, v0);
    runFrame(graph, h, 1, gen1, v1);

    CHECK(gen0 != 0);     // a freshly allocated transient gets a real id
    CHECK(v0 != nullptr);
    CHECK(gen1 == gen0);  // same desc -> pool reuse -> SAME physical texture -> stable generation
    CHECK(v1 == v0);      // the same pooled view comes back (the case the cache optimizes for)
}

TEST_CASE("rg.transient: distinct transients get distinct generations")
{
    Harness h; REQUIRE(h.init());
    RenderGraph graph(&h.device);
    graph.setOutputSize(64, 64);
    graph.beginFrame(0);

    const RGHandle bbH = graph.importTarget(u8"BB", h.bb, h.bbView, rhi::ResourceState::Present);
    const RGHandle a = graph.createTransient(u8"A", RGTextureDesc(rhi::TextureFormat::RGBA16Float, 64, 64));
    const RGHandle b = graph.createTransient(u8"B", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm, 32, 32));

    u64 genA = 0, genB = 0;
    graph.addRenderPass(u8"PA", [&](PassBuilder& pb) {
        pb.setColorTarget(0, a, rhi::LoadOp::Clear, rhi::StoreOp::Store); pb.neverCull();
        pb.setExecute([&](rhi::RenderPassEncoder&) { genA = graph.getTextureGeneration(a); });
    });
    graph.addRenderPass(u8"PB", [&](PassBuilder& pb) {
        pb.setColorTarget(0, b, rhi::LoadOp::Clear, rhi::StoreOp::Store); pb.neverCull();
        pb.setExecute([&](rhi::RenderPassEncoder&) { genB = graph.getTextureGeneration(b); });
    });
    graph.addRenderPass(u8"Sink", [&](PassBuilder& pb) {
        pb.setColorTarget(0, bbH, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        pb.readTexture(a); pb.readTexture(b); pb.neverCull();
        pb.setExecute([](rhi::RenderPassEncoder&) {});
    });

    CHECK(graph.execute(h.enc).isOk());
    CHECK(genA != 0);
    CHECK(genB != 0);
    CHECK(genA != genB);   // two distinct physical allocations -> distinct ids
}
