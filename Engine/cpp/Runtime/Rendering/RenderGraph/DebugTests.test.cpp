#include <doctest_with_main.h>


import core;
import rhi;
import rendergraph;

using namespace draco;
using namespace draco::rendergraph;
namespace rhi = draco::rhi;

namespace
{
    bool contains(std::u8string_view hay, std::u8string_view needle)
    {
        if (needle.size() > hay.size()) { return false; }
        for (usize i = 0; i + needle.size() <= hay.size(); ++i)
        {
            if (hay.substr(i, needle.size()) == needle) { return true; }
        }
        return false;
    }
}

TEST_CASE("rg.debug: ExportDOT produces valid syntax")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle color = graph.createTransient(u8"SceneColor", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));
    const RGHandle depth = graph.createTransient(u8"Depth", RGTextureDesc(rhi::TextureFormat::Depth32Float));

    graph.addRenderPass(u8"DepthPrepass", [&](PassBuilder& b) {
        b.setDepthTarget(depth, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });
    graph.addRenderPass(u8"ForwardOpaque", [&](PassBuilder& b) {
        b.readTexture(depth);
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });

    std::u8string dot;
    GraphDebug::exportDOT(graph, dot);
    const std::u8string_view v = dot;
    CHECK(contains(v, u8"digraph"));
    CHECK(contains(v, u8"DepthPrepass"));
    CHECK(contains(v, u8"ForwardOpaque"));
    CHECK(contains(v, u8"SceneColor"));
    CHECK(contains(v, u8"}"));
}

TEST_CASE("rg.debug: ExportSummary includes counts")
{
    RenderGraph graph(nullptr);
    graph.setOutputSize(1920, 1080);
    graph.beginFrame(0);
    const RGHandle color = graph.createTransient(u8"Color", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));
    graph.addRenderPass(u8"Pass1", [&](PassBuilder& b) {
        b.setColorTarget(0, color, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
    });
    REQUIRE(graph.compile().isOk());

    std::u8string summary;
    GraphDebug::exportSummary(graph, summary);
    const std::u8string_view v = summary;
    CHECK(contains(v, u8"1920x1080"));
    CHECK(contains(v, u8"Pass1"));
    CHECK(contains(v, u8"Render"));
}

TEST_CASE("rg.debug: DOT marks culled passes dashed")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle tex = graph.createTransient(u8"Tex", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));
    graph.addRenderPass(u8"Culled", [&](PassBuilder& b) {
        b.setColorTarget(0, tex, rhi::LoadOp::Clear, rhi::StoreOp::Store);
    });
    REQUIRE(graph.compile().isOk());

    std::u8string dot;
    GraphDebug::exportDOT(graph, dot);
    CHECK(contains(dot, u8"dashed"));
}
