#include <doctest_with_main.h>


import core;
import rhi;
import rendergraph;

using namespace draco;
using namespace draco::rendergraph;
namespace rhi = draco::rhi;

TEST_CASE("rg.cull: unused pass is culled")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle color = graph.createTransient(u8"Color", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));

    graph.addRenderPass(u8"Unused", [&](PassBuilder& b) {
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
    });

    REQUIRE(graph.compile().isOk());
    CHECK(graph.culledPassCount() == 1u);
}

TEST_CASE("rg.cull: NeverCull prevents culling")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle color = graph.createTransient(u8"Color", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));

    graph.addRenderPass(u8"Important", [&](PassBuilder& b) {
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });

    REQUIRE(graph.compile().isOk());
    CHECK(graph.culledPassCount() == 0u);
}

TEST_CASE("rg.cull: HasSideEffects prevents culling")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    graph.addComputePass(u8"SideEffect", [](PassBuilder& b) { b.hasSideEffects(); });

    REQUIRE(graph.compile().isOk());
    CHECK(graph.culledPassCount() == 0u);
}

TEST_CASE("rg.cull: backward propagation keeps dependencies alive")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle depth = graph.createTransient(u8"Depth", RGTextureDesc(rhi::TextureFormat::Depth32Float));
    const RGHandle color = graph.createTransient(u8"Color", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));

    graph.addRenderPass(u8"DepthPrepass", [&](PassBuilder& b) {
        b.setDepthTarget(depth, rhi::LoadOp::Clear, rhi::StoreOp::Store);
    });
    graph.addRenderPass(u8"ForwardOpaque", [&](PassBuilder& b) {
        b.readTexture(depth);
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });

    REQUIRE(graph.compile().isOk());
    CHECK(graph.culledPassCount() == 0u);   // DepthPrepass kept alive by ForwardOpaque
}

TEST_CASE("rg.cull: imported with final state prevents culling")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle backbuffer = graph.importTarget(u8"BB", nullptr, nullptr, rhi::ResourceState::Present);
    const RGHandle color = graph.createTransient(u8"Color", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));

    graph.addRenderPass(u8"Render", [&](PassBuilder& b) {
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
    });
    graph.addRenderPass(u8"Blit", [&](PassBuilder& b) {
        b.readTexture(color);
        b.setColorTarget(0, backbuffer, rhi::LoadOp::Clear, rhi::StoreOp::Store);
    });

    REQUIRE(graph.compile().isOk());
    CHECK(graph.culledPassCount() == 0u);
}
