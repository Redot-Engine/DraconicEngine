#include <doctest_with_main.h>


import core;
import rhi;
import rendergraph;

using namespace draco;
using namespace draco::rendergraph;
namespace rhi = draco::rhi;

TEST_CASE("rg.builder: ReadTexture adds an access")
{
    RenderGraphPass pass(u8"Test", RGPassType::Render);
    PassBuilder builder(pass);
    const RGHandle handle{ 0, 1 };
    builder.readTexture(handle);

    REQUIRE(pass.accesses.size() == 1u);
    CHECK(pass.accesses[0].handle == handle);
    CHECK(pass.accesses[0].type == RGAccessType::ReadTexture);
    CHECK(pass.accesses[0].subresource.isAll());
}

TEST_CASE("rg.builder: ReadTexture with subresource")
{
    RenderGraphPass pass(u8"Test", RGPassType::Render);
    PassBuilder builder(pass);
    builder.readTexture(RGHandle{ 0, 1 }, RGSubresourceRange{ 0, 1, 2, 1 });

    CHECK(pass.accesses[0].subresource.baseArrayLayer == 2u);
    CHECK(pass.accesses[0].subresource.arrayLayerCount == 1u);
}

TEST_CASE("rg.builder: SetColorTarget adds access + attachment")
{
    RenderGraphPass pass(u8"Test", RGPassType::Render);
    PassBuilder builder(pass);
    const RGHandle handle{ 0, 1 };
    builder.setColorTarget(0, handle, rhi::LoadOp::Clear, rhi::StoreOp::Store);

    REQUIRE(pass.accesses.size() == 1u);
    CHECK(pass.accesses[0].type == RGAccessType::WriteColorTarget);
    REQUIRE(pass.colorTargets.size() == 1u);
    CHECK(pass.colorTargets[0].handle == handle);
    CHECK(pass.colorTargets[0].loadOp == rhi::LoadOp::Clear);
}

TEST_CASE("rg.builder: SetDepthTarget adds access + attachment")
{
    RenderGraphPass pass(u8"Test", RGPassType::Render);
    PassBuilder builder(pass);
    const RGHandle handle{ 0, 1 };
    builder.setDepthTarget(handle, rhi::LoadOp::Clear, rhi::StoreOp::Store, 1.0f);

    REQUIRE(pass.accesses.size() == 1u);
    CHECK(pass.accesses[0].type == RGAccessType::WriteDepthTarget);
    REQUIRE(pass.depthTarget.has_value());
    CHECK(pass.depthTarget.value().handle == handle);
    CHECK(pass.depthTarget.value().depthClearValue == 1.0f);
}

TEST_CASE("rg.builder: ReadDepth sets read-only")
{
    RenderGraphPass pass(u8"Test", RGPassType::Render);
    PassBuilder builder(pass);
    builder.readDepth(RGHandle{ 0, 1 });

    REQUIRE(pass.depthTarget.has_value());
    CHECK(pass.depthTarget.value().readOnly);
    REQUIRE(pass.accesses.size() == 1u);
    CHECK(pass.accesses[0].type == RGAccessType::ReadDepthStencil);
}

TEST_CASE("rg.builder: SampleDepth reads without an attachment")
{
    RenderGraphPass pass(u8"Test", RGPassType::Render);
    PassBuilder builder(pass);
    builder.sampleDepth(RGHandle{ 0, 1 });

    // Unlike ReadDepth, sampling a depth texture in a shader is NOT a depth attachment:
    // it adds a read access only, leaving depthTarget unset.
    CHECK_FALSE(pass.depthTarget.has_value());
    REQUIRE(pass.accesses.size() == 1u);
    CHECK(pass.accesses[0].type == RGAccessType::SampleDepthStencil);
    CHECK(pass.accesses[0].isRead());
    CHECK_FALSE(pass.accesses[0].isWrite());
}

TEST_CASE("rg.builder: SampleDepth with subresource")
{
    RenderGraphPass pass(u8"Test", RGPassType::Render);
    PassBuilder builder(pass);
    builder.sampleDepth(RGHandle{ 0, 1 }, RGSubresourceRange{ 0, 1, 3, 2 });

    REQUIRE(pass.accesses.size() == 1u);
    CHECK(pass.accesses[0].subresource.baseArrayLayer == 3u);
    CHECK(pass.accesses[0].subresource.arrayLayerCount == 2u);
}

TEST_CASE("rg.builder: NeverCull / HasSideEffects flags")
{
    {
        RenderGraphPass pass(u8"Test", RGPassType::Render);
        PassBuilder(pass).neverCull();
        CHECK(pass.neverCull);
        CHECK(pass.shouldSurviveCulling());
    }
    {
        RenderGraphPass pass(u8"Test", RGPassType::Render);
        PassBuilder(pass).hasSideEffects();
        CHECK(pass.hasSideEffects);
        CHECK(pass.shouldSurviveCulling());
    }
}

TEST_CASE("rg.builder: EnableIf stores a runtime condition")
{
    RenderGraphPass pass(u8"Test", RGPassType::Render);
    PassBuilder(pass).enableIf([]() { return true; });
    REQUIRE(static_cast<bool>(pass.condition));
    CHECK(pass.condition());
}

TEST_CASE("rg.builder: storage + copy accesses")
{
    {
        RenderGraphPass pass(u8"Test", RGPassType::Compute);
        PassBuilder(pass).writeStorage(RGHandle{ 0, 1 });
        REQUIRE(pass.accesses.size() == 1u);
        CHECK(pass.accesses[0].type == RGAccessType::WriteStorage);
    }
    {
        RenderGraphPass pass(u8"Test", RGPassType::Compute);
        PassBuilder(pass).readWriteStorage(RGHandle{ 0, 1 });
        REQUIRE(pass.accesses.size() == 1u);
        CHECK(pass.accesses[0].type == RGAccessType::ReadWriteStorage);
        CHECK(pass.accesses[0].isRead());
        CHECK(pass.accesses[0].isWrite());
    }
    {
        RenderGraphPass pass(u8"Test", RGPassType::Copy);
        PassBuilder(pass).copySrc(RGHandle{ 0, 1 }).copyDst(RGHandle{ 1, 1 });
        REQUIRE(pass.accesses.size() == 2u);
        CHECK(pass.accesses[0].type == RGAccessType::ReadCopySrc);
        CHECK(pass.accesses[1].type == RGAccessType::WriteCopyDst);
    }
}

TEST_CASE("rg.builder: fluent chaining")
{
    RenderGraphPass pass(u8"Test", RGPassType::Render);
    PassBuilder(pass)
        .readTexture(RGHandle{ 2, 1 })
        .setColorTarget(0, RGHandle{ 0, 1 }, rhi::LoadOp::Clear, rhi::StoreOp::Store)
        .setDepthTarget(RGHandle{ 1, 1 }, rhi::LoadOp::Load, rhi::StoreOp::Store)
        .neverCull();

    CHECK(pass.accesses.size() == 3u);   // read + write-color + readwrite-depth (Load+Store)
    CHECK(pass.colorTargets.size() == 1u);
    CHECK(pass.depthTarget.has_value());
    CHECK(pass.neverCull);
}

TEST_CASE("rg.builder: GetInputs folds LoadOp into a read")
{
    RenderGraphPass pass(u8"Test", RGPassType::Render);
    const RGHandle handle{ 0, 1 };
    PassBuilder(pass).setColorTarget(0, handle, rhi::LoadOp::Load, rhi::StoreOp::Store);

    std::vector<RGResourceAccess> inputs;
    pass.getInputs(inputs);
    bool hasRead = false;
    for (const RGResourceAccess& input : inputs)
    {
        if (input.handle == handle && input.isRead()) { hasRead = true; }
    }
    CHECK(hasRead);
}
