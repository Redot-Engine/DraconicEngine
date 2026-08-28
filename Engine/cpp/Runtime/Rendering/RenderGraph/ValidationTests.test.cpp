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

    bool hasSeverity(const std::vector<ValidationMessage>& messages, ValidationSeverity severity)
    {
        for (const ValidationMessage& m : messages) { if (m.severity == severity) { return true; } }
        return false;
    }
}

TEST_CASE("rg.validation: uninitialized read is an error")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle tex = graph.createTransient(u8"Tex", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));
    graph.addRenderPass(u8"BadPass", [&](PassBuilder& b) { b.readTexture(tex); b.neverCull(); });

    std::vector<ValidationMessage> messages;
    GraphValidator::validate(graph, messages);
    CHECK(hasSeverity(messages, ValidationSeverity::Error));
}

TEST_CASE("rg.validation: reading an imported resource is fine")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle imported = graph.importTarget(u8"External", nullptr, nullptr);
    graph.addRenderPass(u8"ReadImported", [&](PassBuilder& b) { b.readTexture(imported); b.neverCull(); });

    std::vector<ValidationMessage> messages;
    GraphValidator::validate(graph, messages);
    CHECK_FALSE(hasSeverity(messages, ValidationSeverity::Error));
}

TEST_CASE("rg.validation: empty pass is a warning")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    graph.addRenderPass(u8"Empty", [](PassBuilder& b) { b.neverCull(); });

    std::vector<ValidationMessage> messages;
    GraphValidator::validate(graph, messages);
    CHECK(hasSeverity(messages, ValidationSeverity::Warning));
}

TEST_CASE("rg.validation: redundant write is a warning")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle tex = graph.createTransient(u8"Tex", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));
    graph.addRenderPass(u8"Write1", [&](PassBuilder& b) { b.setColorTarget(0, tex, rhi::LoadOp::Clear, rhi::StoreOp::Store); b.neverCull(); });
    graph.addRenderPass(u8"Write2", [&](PassBuilder& b) { b.setColorTarget(0, tex, rhi::LoadOp::Clear, rhi::StoreOp::Store); b.neverCull(); });

    std::vector<ValidationMessage> messages;
    GraphValidator::validate(graph, messages);
    CHECK(hasSeverity(messages, ValidationSeverity::Warning));
}

TEST_CASE("rg.validation: a clean graph produces no messages")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle tex = graph.createTransient(u8"Tex", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));
    graph.addRenderPass(u8"Write", [&](PassBuilder& b) {
        b.setColorTarget(0, tex, rhi::LoadOp::Clear, rhi::StoreOp::Store);
        b.neverCull();
        b.setExecute([](rhi::RenderPassEncoder&) {});
    });
    graph.addRenderPass(u8"Read", [&](PassBuilder& b) {
        b.readTexture(tex);
        b.neverCull();
        b.setExecute([](rhi::RenderPassEncoder&) {});
    });

    std::vector<ValidationMessage> messages;
    GraphValidator::validate(graph, messages);
    CHECK(messages.size() == 0u);
}

TEST_CASE("rg.validation: ValidateToString formats output")
{
    RenderGraph graph(nullptr);
    graph.beginFrame(0);
    const RGHandle tex = graph.createTransient(u8"Tex", RGTextureDesc(rhi::TextureFormat::RGBA8Unorm));
    graph.addRenderPass(u8"BadRead", [&](PassBuilder& b) { b.readTexture(tex); b.neverCull(); });

    std::u8string result;
    GraphValidator::validateToString(graph, result);
    CHECK(contains(result, u8"issue"));
}
