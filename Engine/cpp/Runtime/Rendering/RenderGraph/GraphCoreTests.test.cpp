#include <doctest_with_main.h>


import core;
import rhi;
import rendergraph;

using namespace draco;
using namespace draco::rendergraph;
namespace rhi = draco::rhi;

TEST_CASE("rg.graph: create transient returns a valid handle")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle handle = graph.createTransient(u8"Test", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm, SizeMode::FullSize));
    CHECK(handle.isValid());
    CHECK(graph.resourceCount() == 1u);
}

TEST_CASE("rg.graph: multiple resources get unique handles")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle h1 = graph.createTransient(u8"A", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));
    const RGHandle h2 = graph.createTransient(u8"B", RGTextureDesc(rhi::TextureFormat::Depth32Float));
    CHECK(h1 != h2);
    CHECK(graph.resourceCount() == 2u);
}

TEST_CASE("rg.graph: get resource by name")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle h1 = graph.createTransient(u8"SceneColor", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));
    CHECK(graph.getResource(u8"SceneColor") == h1);
    CHECK_FALSE(graph.getResource(u8"NonExistent").isValid());
}

TEST_CASE("rg.graph: pass count is correct")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle color = graph.createTransient(u8"Color", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));

    graph.addRenderPass(u8"Pass1", [&](PassBuilder& b) {
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });
    graph.addComputePass(u8"Pass2", [](PassBuilder& b) { b.hasSideEffects(); });

    CHECK(graph.passCount() == 2u);
}

TEST_CASE("rg.graph: set output size affects resolution")
{
    RenderGraph graph(nullptr);
    graph.setOutputSize(1920, 1080);
    CHECK(graph.outputWidth() == 1920u);
    CHECK(graph.outputHeight() == 1080u);
}

TEST_CASE("rg.graph: import target with final state")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle handle = graph.importTarget(u8"Backbuffer", nullptr, nullptr, rhi::ResourceState::Present);
    CHECK(handle.isValid());
}

TEST_CASE("rg.graph: reset keeps persistent, drops transient")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    graph.registerPersistent(u8"Shadow", nullptr, nullptr);
    graph.createTransient(u8"Temp", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));

    graph.reset();

    CHECK(graph.getResource(u8"Shadow").isValid());
    CHECK_FALSE(graph.getResource(u8"Temp").isValid());
}

TEST_CASE("rg.graph: SetViewport records a per-pass viewport override")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle color = graph.createTransient(u8"Color", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));

    graph.addRenderPass(u8"ViewportPass", [&](PassBuilder& b) {
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.setViewport(10, 20, 100, 200);
        b.neverCull();
    });

    REQUIRE(graph.passes().size() == 1u);
    const RenderGraphPass* pass = graph.passes()[0];
    CHECK(pass->hasViewport);
    CHECK(pass->viewportX == 10);
    CHECK(pass->viewportY == 20);
    CHECK(pass->viewportW == 100u);
    CHECK(pass->viewportH == 200u);
}

TEST_CASE("rg.graph: a pass without SetViewport has no viewport override (full attachment)")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle color = graph.createTransient(u8"Color", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));

    graph.addRenderPass(u8"FullPass", [&](PassBuilder& b) {
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });

    REQUIRE(graph.passes().size() == 1u);
    CHECK_FALSE(graph.passes()[0]->hasViewport);
}
