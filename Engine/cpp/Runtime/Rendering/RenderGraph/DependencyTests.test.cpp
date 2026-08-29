#include <doctest_with_main.h>


import core;
import rhi;
import rendergraph;

using namespace draco;
using namespace draco::rendergraph;
namespace rhi = draco::rhi;

namespace
{
    std::u8string_view passName(RenderGraph& g, i32 orderSlot)
    {
        return g.passes()[static_cast<usize>(g.executionOrder()[static_cast<usize>(orderSlot)])]->name;
    }
}

TEST_CASE("rg.dep: reader depends on writer")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle tex = graph.createTransient(u8"Tex", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));

    graph.addRenderPass(u8"Writer", [&](PassBuilder& b) {
        b.setColorTarget(0, tex, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });
    graph.addRenderPass(u8"Reader", [&](PassBuilder& b) {
        b.readTexture(tex);
        b.neverCull();
    });

    REQUIRE(graph.compile().isOk());
    REQUIRE(graph.executionOrder().size() == 2u);
    CHECK(passName(graph, 0) == u8"Writer");
}

TEST_CASE("rg.dep: multiple readers fan out, writer first")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle tex = graph.createTransient(u8"Tex", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));

    graph.addRenderPass(u8"Writer", [&](PassBuilder& b) {
        b.setColorTarget(0, tex, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });
    graph.addRenderPass(u8"ReaderA", [&](PassBuilder& b) { b.readTexture(tex); b.neverCull(); });
    graph.addRenderPass(u8"ReaderB", [&](PassBuilder& b) { b.readTexture(tex); b.neverCull(); });

    REQUIRE(graph.compile().isOk());
    REQUIRE(graph.executionOrder().size() == 3u);
    CHECK(passName(graph, 0) == u8"Writer");
}

TEST_CASE("rg.dep: subresource writes are independent, reader last")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    RGTextureDesc atlasDesc(rhi::TextureFormat::Depth32Float);
    atlasDesc.arrayLayerCount = 4;
    const RGHandle atlas = graph.createTransient(u8"ShadowAtlas", atlasDesc);

    graph.addRenderPass(u8"Cascade0", [&](PassBuilder& b) {
        b.setDepthTarget(atlas, rhi::LoadOp::Clear, rhi::StoreOp::Store, 1.0f, RGSubresourceRange{ 0, 1, 0, 1 });
        b.neverCull();
    });
    graph.addRenderPass(u8"Cascade1", [&](PassBuilder& b) {
        b.setDepthTarget(atlas, rhi::LoadOp::Clear, rhi::StoreOp::Store, 1.0f, RGSubresourceRange{ 0, 1, 1, 1 });
        b.neverCull();
    });
    graph.addRenderPass(u8"Forward", [&](PassBuilder& b) { b.readTexture(atlas); b.neverCull(); });

    REQUIRE(graph.compile().isOk());
    REQUIRE(graph.executionOrder().size() == 3u);
    CHECK(passName(graph, 2) == u8"Forward");
}

TEST_CASE("rg.dep: LoadOp on a color target creates a dependency on the writer")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle color = graph.createTransient(u8"SceneColor", RGTextureDesc(rhi::TextureFormat::RGBA16Float));

    graph.addRenderPass(u8"ForwardOpaque", [&](PassBuilder& b) {
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });
    graph.addRenderPass(u8"Terrain", [&](PassBuilder& b) {
        b.setColorTarget(0, color, rhi::LoadOp::Load, rhi::StoreOp::Store);
        b.neverCull();
    });

    REQUIRE(graph.compile().isOk());
    REQUIRE(graph.executionOrder().size() == 2u);
    CHECK(passName(graph, 0) == u8"ForwardOpaque");
    CHECK(passName(graph, 1) == u8"Terrain");
}

TEST_CASE("rg.dep: LoadOp on a depth target creates a dependency on the writer")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle depth = graph.createTransient(u8"Depth", RGTextureDesc(rhi::TextureFormat::Depth32Float));

    graph.addRenderPass(u8"DepthPrepass", [&](PassBuilder& b) {
        b.setDepthTarget(depth, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });
    graph.addRenderPass(u8"ForwardOpaque", [&](PassBuilder& b) {
        b.setDepthTarget(depth, rhi::LoadOp::Load, rhi::StoreOp::Store);
        b.neverCull();
    });

    REQUIRE(graph.compile().isOk());
    REQUIRE(graph.executionOrder().size() == 2u);
    CHECK(passName(graph, 0) == u8"DepthPrepass");
    CHECK(passName(graph, 1) == u8"ForwardOpaque");
}

TEST_CASE("rg.dep: writer chain orders correctly")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle tex = graph.createTransient(u8"Tex", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));

    graph.addRenderPass(u8"Write1", [&](PassBuilder& b) {
        b.setColorTarget(0, tex, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });
    graph.addComputePass(u8"Process", [&](PassBuilder& b) {
        b.readTexture(tex);
        b.writeStorage(tex);
        b.neverCull();
    });
    graph.addRenderPass(u8"FinalRead", [&](PassBuilder& b) { b.readTexture(tex); b.neverCull(); });

    REQUIRE(graph.compile().isOk());
    REQUIRE(graph.executionOrder().size() == 3u);
    CHECK(passName(graph, 0) == u8"Write1");
    CHECK(passName(graph, 1) == u8"Process");
    CHECK(passName(graph, 2) == u8"FinalRead");
}
