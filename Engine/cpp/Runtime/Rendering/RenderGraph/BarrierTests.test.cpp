// BarrierSolver directly via a recording mock encoder and plain RHI textures
// (rhi::Texture is concrete, so no texture mock is needed; its pointer identity
// is what the solver keys on).

#include <doctest_with_main.h>
#include <memory>


import core;
import rhi;
import rendergraph;

using namespace draco;
using namespace draco::rendergraph;
namespace rhi = draco::rhi;
using RS = rhi::ResourceState;
using AT = RGAccessType;

namespace
{
    // Records emitted barriers; stubs the rest of the CommandEncoder interface.
    class MockEncoder final : public rhi::CommandEncoder
    {
    public:
        std::vector<rhi::TextureBarrier> textureBarriers;
        std::vector<rhi::BufferBarrier> bufferBarriers;

        void barrier(const rhi::BarrierGroup& group) override
        {
            for (usize i = 0; i < group.textureBarriers.size(); ++i) { textureBarriers.push_back(group.textureBarriers[i]); }
            for (usize i = 0; i < group.bufferBarriers.size(); ++i) { bufferBarriers.push_back(group.bufferBarriers[i]); }
        }

        rhi::RenderPassEncoder* beginRenderPass(const rhi::RenderPassDesc&) override { return nullptr; }
        rhi::ComputePassEncoder* beginComputePass(std::u8string_view) override { return nullptr; }
        rhi::RenderBundleEncoder* createRenderBundleEncoder(const rhi::RenderBundleDesc&) override { return nullptr; }
        void copyBufferToBuffer(rhi::Buffer*, u64, rhi::Buffer*, u64, u64) override {}
        void copyBufferToTexture(rhi::Buffer*, rhi::Texture*, const rhi::BufferTextureCopyRegion&) override {}
        void copyTextureToBuffer(rhi::Texture*, rhi::Buffer*, const rhi::BufferTextureCopyRegion&) override {}
        void copyTextureToTexture(rhi::Texture*, rhi::Texture*, const rhi::TextureCopyRegion&) override {}
        void blit(rhi::Texture*, rhi::Texture*) override {}
        void generateMipmaps(rhi::Texture*) override {}
        void resolveTexture(rhi::Texture*, rhi::Texture*) override {}
        void resetQuerySet(rhi::QuerySet*, u32, u32) override {}
        void writeTimestamp(rhi::QuerySet*, u32) override {}
        void resolveQuerySet(rhi::QuerySet*, u32, u32, rhi::Buffer*, u64) override {}
        void beginDebugLabel(std::u8string_view, f32, f32, f32, f32) override {}
        void endDebugLabel() override {}
        void insertDebugLabel(std::u8string_view, f32, f32, f32, f32) override {}
        rhi::CommandBuffer* finish() override { return nullptr; }
    };

    rhi::Texture makeTexture(RS initialState, u32 mips = 1, u32 layers = 1)
    {
        rhi::Texture tex;
        tex.desc.mipLevelCount = mips;
        tex.desc.arrayLayerCount = layers;
        tex.initialState = initialState;
        return tex;
    }

    RGResourceAccess access(u32 index, AT type, RGSubresourceRange sub = {})
    {
        return RGResourceAccess{ RGHandle{ index, 0 }, type, sub };
    }
}

TEST_CASE("barriers: same texture via two handles emits one transition")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture shadow = makeTexture(RS::Undefined);

    RenderGraphResource res0(u8"ShadowWrite", RGResourceType::Texture, RGResourceLifetime::Imported);
    res0.texture = &shadow;
    RenderGraphResource res1(u8"ShadowRead", RGResourceType::Texture, RGResourceLifetime::Imported);
    res1.texture = &shadow; // same GPU texture
    RenderGraphResource* resources[] = { &res0, &res1 };
    const std::span<RenderGraphResource* const> span(resources, 2);

    solver.reset(span);

    RenderGraphPass writePass(u8"ShadowPass", RGPassType::Render);
    writePass.accesses.push_back(access(0, AT::WriteDepthTarget));
    solver.emitBarriers(writePass, span, encoder);
    encoder.textureBarriers.clear();

    RenderGraphPass readPass(u8"ForwardPass", RGPassType::Render);
    readPass.accesses.push_back(access(1, AT::ReadTexture));
    solver.emitBarriers(readPass, span, encoder);

    REQUIRE(encoder.textureBarriers.size() == 1u);
    CHECK(encoder.textureBarriers[0].oldState == RS::DepthStencilWrite);
    CHECK(encoder.textureBarriers[0].newState == RS::ShaderRead);
    CHECK(encoder.textureBarriers[0].texture == &shadow);
}

TEST_CASE("barriers: single handle read-after-write")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined);

    RenderGraphResource res(u8"Color", RGResourceType::Texture, RGResourceLifetime::Transient);
    res.texture = &tex;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass writePass(u8"Writer", RGPassType::Render);
    writePass.accesses.push_back(access(0, AT::WriteColorTarget));
    solver.emitBarriers(writePass, span, encoder);
    encoder.textureBarriers.clear();

    RenderGraphPass readPass(u8"Reader", RGPassType::Render);
    readPass.accesses.push_back(access(0, AT::ReadTexture));
    solver.emitBarriers(readPass, span, encoder);

    REQUIRE(encoder.textureBarriers.size() == 1u);
    CHECK(encoder.textureBarriers[0].oldState == RS::RenderTarget);
    CHECK(encoder.textureBarriers[0].newState == RS::ShaderRead);
}

TEST_CASE("barriers: compute write then render read")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined);

    RenderGraphResource res(u8"Volume", RGResourceType::Texture, RGResourceLifetime::Imported);
    res.texture = &tex;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass computePass(u8"Compute", RGPassType::Compute);
    computePass.accesses.push_back(access(0, AT::WriteStorage));
    solver.emitBarriers(computePass, span, encoder);
    encoder.textureBarriers.clear();

    RenderGraphPass renderPass(u8"Render", RGPassType::Render);
    renderPass.accesses.push_back(access(0, AT::ReadTexture));
    solver.emitBarriers(renderPass, span, encoder);

    REQUIRE(encoder.textureBarriers.size() == 1u);
    CHECK(encoder.textureBarriers[0].oldState == RS::ShaderWrite);
    CHECK(encoder.textureBarriers[0].newState == RS::ShaderRead);
}

TEST_CASE("barriers: final transition uses texture-keyed state")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined);

    RenderGraphResource res(u8"Backbuffer", RGResourceType::Texture, RGResourceLifetime::Imported);
    res.texture = &tex;
    res.finalState = RS::Present;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass pass(u8"FinalBlit", RGPassType::Render);
    pass.accesses.push_back(access(0, AT::WriteColorTarget));
    solver.emitBarriers(pass, span, encoder);
    encoder.textureBarriers.clear();

    solver.emitFinalTransitions(span, encoder);

    REQUIRE(encoder.textureBarriers.size() == 1u);
    CHECK(encoder.textureBarriers[0].oldState == RS::RenderTarget);
    CHECK(encoder.textureBarriers[0].newState == RS::Present);
}

TEST_CASE("barriers: none when already in the correct state")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined);

    RenderGraphResource res(u8"Tex", RGResourceType::Texture, RGResourceLifetime::Imported);
    res.texture = &tex;
    res.lastKnownState = RS::ShaderRead;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass pass(u8"Reader", RGPassType::Render);
    pass.accesses.push_back(access(0, AT::ReadTexture));
    solver.emitBarriers(pass, span, encoder);

    CHECK(encoder.textureBarriers.size() == 0u);
}

TEST_CASE("barriers: per-layer writes emit individual barriers")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined, 1, 4); // 4 cascades

    RenderGraphResource res(u8"ShadowArray", RGResourceType::Texture, RGResourceLifetime::Imported);
    res.texture = &tex;
    res.lastKnownState = RS::ShaderRead;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass pass0(u8"Cascade0", RGPassType::Render);
    pass0.accesses.push_back(access(0, AT::WriteDepthTarget, RGSubresourceRange{ 0, 1, 0, 1 }));
    solver.emitBarriers(pass0, span, encoder);
    REQUIRE(encoder.textureBarriers.size() == 1u);
    CHECK(encoder.textureBarriers[0].oldState == RS::ShaderRead);
    CHECK(encoder.textureBarriers[0].newState == RS::DepthStencilWrite);
    CHECK(encoder.textureBarriers[0].baseArrayLayer == 0u);
    CHECK(encoder.textureBarriers[0].arrayLayerCount == 1u);
    encoder.textureBarriers.clear();

    RenderGraphPass pass2(u8"Cascade2", RGPassType::Render);
    pass2.accesses.push_back(access(0, AT::WriteDepthTarget, RGSubresourceRange{ 0, 1, 2, 1 }));
    solver.emitBarriers(pass2, span, encoder);
    REQUIRE(encoder.textureBarriers.size() == 1u);
    CHECK(encoder.textureBarriers[0].baseArrayLayer == 2u);
    CHECK(encoder.textureBarriers[0].arrayLayerCount == 1u);
}

TEST_CASE("barriers: non-uniform whole-resource read emits per-subresource")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined, 1, 2);

    RenderGraphResource res(u8"Tex", RGResourceType::Texture, RGResourceLifetime::Imported);
    res.texture = &tex;
    res.lastKnownState = RS::ShaderRead;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass writePass(u8"Writer", RGPassType::Render);
    writePass.accesses.push_back(access(0, AT::WriteColorTarget, RGSubresourceRange{ 0, 1, 0, 1 }));
    solver.emitBarriers(writePass, span, encoder);
    encoder.textureBarriers.clear();

    RenderGraphPass readPass(u8"Reader", RGPassType::Render);
    readPass.accesses.push_back(access(0, AT::ReadTexture));
    solver.emitBarriers(readPass, span, encoder);

    REQUIRE(encoder.textureBarriers.size() == 1u); // only layer 0 transitions
    CHECK(encoder.textureBarriers[0].oldState == RS::RenderTarget);
    CHECK(encoder.textureBarriers[0].newState == RS::ShaderRead);
    CHECK(encoder.textureBarriers[0].baseArrayLayer == 0u);
}

TEST_CASE("barriers: all layers written collapses to uniform")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined, 1, 2);

    RenderGraphResource res(u8"Tex", RGResourceType::Texture, RGResourceLifetime::Imported);
    res.texture = &tex;
    res.lastKnownState = RS::ShaderRead;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass p0(u8"W0", RGPassType::Render);
    p0.accesses.push_back(access(0, AT::WriteColorTarget, RGSubresourceRange{ 0, 1, 0, 1 }));
    solver.emitBarriers(p0, span, encoder);
    encoder.textureBarriers.clear();

    RenderGraphPass p1(u8"W1", RGPassType::Render);
    p1.accesses.push_back(access(0, AT::WriteColorTarget, RGSubresourceRange{ 0, 1, 1, 1 }));
    solver.emitBarriers(p1, span, encoder);
    encoder.textureBarriers.clear();

    RenderGraphPass readPass(u8"Read", RGPassType::Render);
    readPass.accesses.push_back(access(0, AT::ReadTexture));
    solver.emitBarriers(readPass, span, encoder);

    REQUIRE(encoder.textureBarriers.size() == 1u); // whole-resource fast path
    CHECK(encoder.textureBarriers[0].oldState == RS::RenderTarget);
    CHECK(encoder.textureBarriers[0].newState == RS::ShaderRead);
    CHECK(encoder.textureBarriers[0].mipLevelCount == 0xFFFFFFFFu);
    CHECK(encoder.textureBarriers[0].arrayLayerCount == 0xFFFFFFFFu);
}

TEST_CASE("barriers: SampleDepth on a written cascade array reads as DepthStencilRead")
{
    // The real CSM scenario: each cascade writes one layer (DepthStencilWrite), then the
    // forward pass samples the WHOLE array. SampleDepthStencil must transition it to
    // DepthStencilRead (DEPTH_STENCIL_READ_ONLY_OPTIMAL) - the layout a depth sampler needs -
    // not ShaderRead. After all layers are written uniform, the read is a whole-resource barrier.
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined, 1, 4); // 4 cascades

    RenderGraphResource res(u8"ShadowArray", RGResourceType::Texture, RGResourceLifetime::Imported);
    res.texture = &tex;
    res.lastKnownState = RS::DepthStencilRead;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    for (u32 layer = 0; layer < 4; ++layer)
    {
        RenderGraphPass cascade(u8"Cascade", RGPassType::Render);
        cascade.accesses.push_back(access(0, AT::WriteDepthTarget, RGSubresourceRange{ 0, 1, layer, 1 }));
        solver.emitBarriers(cascade, span, encoder);
    }
    encoder.textureBarriers.clear();

    RenderGraphPass forward(u8"Forward", RGPassType::Render);
    forward.accesses.push_back(access(0, AT::SampleDepthStencil));
    solver.emitBarriers(forward, span, encoder);

    REQUIRE(encoder.textureBarriers.size() == 1u); // whole-resource (all layers uniform)
    CHECK(encoder.textureBarriers[0].oldState == RS::DepthStencilWrite);
    CHECK(encoder.textureBarriers[0].newState == RS::DepthStencilRead);
    CHECK(encoder.textureBarriers[0].arrayLayerCount == 0xFFFFFFFFu);
}

TEST_CASE("barriers: per-mip different states")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined, 4, 1);

    RenderGraphResource res(u8"Tex", RGResourceType::Texture, RGResourceLifetime::Imported);
    res.texture = &tex;
    res.lastKnownState = RS::Undefined;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass p0(u8"WriteMip0", RGPassType::Render);
    p0.accesses.push_back(access(0, AT::WriteColorTarget, RGSubresourceRange{ 0, 1, 0, 1 }));
    solver.emitBarriers(p0, span, encoder);
    encoder.textureBarriers.clear();

    RenderGraphPass p1(u8"WriteMip1", RGPassType::Render);
    p1.accesses.push_back(access(0, AT::WriteColorTarget, RGSubresourceRange{ 1, 1, 0, 1 }));
    solver.emitBarriers(p1, span, encoder);

    REQUIRE(encoder.textureBarriers.size() == 1u);
    CHECK(encoder.textureBarriers[0].oldState == RS::Undefined);
    CHECK(encoder.textureBarriers[0].newState == RS::RenderTarget);
    CHECK(encoder.textureBarriers[0].baseMipLevel == 1u);
    CHECK(encoder.textureBarriers[0].mipLevelCount == 1u);
}

TEST_CASE("barriers: readable-after-write is subresource aware")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined, 1, 4);

    RenderGraphResource res(u8"Tex", RGResourceType::Texture, RGResourceLifetime::Imported);
    res.texture = &tex;
    res.lastKnownState = RS::ShaderRead;
    res.readableAfterWrite = true;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass writePass(u8"Writer", RGPassType::Render);
    writePass.accesses.push_back(access(0, AT::WriteDepthTarget, RGSubresourceRange{ 0, 1, 1, 1 }));
    solver.emitBarriers(writePass, span, encoder);
    encoder.textureBarriers.clear();

    solver.emitReadableAfterWriteBarriers(writePass, span, encoder);

    REQUIRE(encoder.textureBarriers.size() == 1u);
    CHECK(encoder.textureBarriers[0].oldState == RS::DepthStencilWrite);
    CHECK(encoder.textureBarriers[0].newState == RS::ShaderRead);
    CHECK(encoder.textureBarriers[0].baseArrayLayer == 1u);
    CHECK(encoder.textureBarriers[0].arrayLayerCount == 1u);
}

TEST_CASE("barriers: final transition non-uniform emits per-subresource")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined, 1, 2);

    RenderGraphResource res(u8"Swapchain", RGResourceType::Texture, RGResourceLifetime::Imported);
    res.texture = &tex;
    res.lastKnownState = RS::Undefined;
    res.finalState = RS::Present;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass writePass(u8"Blit", RGPassType::Render);
    writePass.accesses.push_back(access(0, AT::WriteColorTarget, RGSubresourceRange{ 0, 1, 0, 1 }));
    solver.emitBarriers(writePass, span, encoder);
    encoder.textureBarriers.clear();

    solver.emitFinalTransitions(span, encoder);

    REQUIRE(encoder.textureBarriers.size() == 2u);
    bool foundLayer0 = false, foundLayer1 = false;
    for (usize i = 0; i < encoder.textureBarriers.size(); ++i)
    {
        const rhi::TextureBarrier& b = encoder.textureBarriers[i];
        if (b.baseArrayLayer == 0 && b.oldState == RS::RenderTarget && b.newState == RS::Present) { foundLayer0 = true; }
        if (b.baseArrayLayer == 1 && b.oldState == RS::Undefined && b.newState == RS::Present) { foundLayer1 = true; }
    }
    CHECK(foundLayer0);
    CHECK(foundLayer1);
}

TEST_CASE("barriers: non-overlapping subresource access emits no false barrier")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined, 1, 4);

    RenderGraphResource res(u8"Array", RGResourceType::Texture, RGResourceLifetime::Imported);
    res.texture = &tex;
    res.lastKnownState = RS::ShaderRead;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass writePass(u8"WriteLayer0", RGPassType::Render);
    writePass.accesses.push_back(access(0, AT::WriteColorTarget, RGSubresourceRange{ 0, 1, 0, 1 }));
    solver.emitBarriers(writePass, span, encoder);
    encoder.textureBarriers.clear();

    RenderGraphPass readPass(u8"ReadLayer2", RGPassType::Render);
    readPass.accesses.push_back(access(0, AT::ReadTexture, RGSubresourceRange{ 0, 1, 2, 1 }));
    solver.emitBarriers(readPass, span, encoder);

    CHECK(encoder.textureBarriers.size() == 0u);
}

TEST_CASE("barriers: persistent resource preserves per-subresource state")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined, 1, 2);

    RenderGraphResource res(u8"Persistent", RGResourceType::Texture, RGResourceLifetime::Persistent);
    res.texture = &tex;
    res.persistentData = std::make_unique<PersistentResource>(&tex, static_cast<rhi::TextureView*>(nullptr));
    res.persistentData->firstFrame = false;
    res.persistentData->lastKnownState = RS::ShaderRead;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass writePass(u8"Writer", RGPassType::Render);
    writePass.accesses.push_back(access(0, AT::WriteColorTarget, RGSubresourceRange{ 0, 1, 0, 1 }));
    solver.emitBarriers(writePass, span, encoder);

    solver.updatePersistentStates(span);

    CHECK(res.persistentData->subresourceStates.size() == 2u);
}

TEST_CASE("barriers: transient first access emits Undefined -> RenderTarget")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined);

    RenderGraphResource res(u8"PipelineOutput", RGResourceType::Texture, RGResourceLifetime::Transient);
    res.texture = &tex;
    RenderGraphResource* resources[] = { &res };
    const std::span<RenderGraphResource* const> span(resources, 1);
    solver.reset(span);

    RenderGraphPass writePass(u8"ForwardOpaque", RGPassType::Render);
    writePass.accesses.push_back(access(0, AT::WriteColorTarget));
    solver.emitBarriers(writePass, span, encoder);

    REQUIRE(encoder.textureBarriers.size() == 1u);
    CHECK(encoder.textureBarriers[0].oldState == RS::Undefined);
    CHECK(encoder.textureBarriers[0].newState == RS::RenderTarget);
}

TEST_CASE("barriers: transient reused across frames starts from Undefined")
{
    BarrierSolver solver;
    MockEncoder encoder;
    rhi::Texture tex = makeTexture(RS::Undefined); // same pooled texture both frames

    // Frame 1
    {
        RenderGraphResource res(u8"PipelineOutput", RGResourceType::Texture, RGResourceLifetime::Transient);
        res.texture = &tex;
        RenderGraphResource* resources[] = { &res };
        const std::span<RenderGraphResource* const> span(resources, 1);
        solver.reset(span);

        RenderGraphPass writePass(u8"ForwardOpaque", RGPassType::Render);
        writePass.accesses.push_back(access(0, AT::WriteColorTarget));
        solver.emitBarriers(writePass, span, encoder);
        encoder.textureBarriers.clear();

        RenderGraphPass readPass(u8"PostProcess", RGPassType::Render);
        readPass.accesses.push_back(access(0, AT::ReadTexture));
        solver.emitBarriers(readPass, span, encoder);
        REQUIRE(encoder.textureBarriers.size() == 1u);
        CHECK(encoder.textureBarriers[0].oldState == RS::RenderTarget);
        CHECK(encoder.textureBarriers[0].newState == RS::ShaderRead);
        encoder.textureBarriers.clear();
    }

    // Frame 2: same texture, fresh resource - must restart from Undefined.
    {
        RenderGraphResource res(u8"PipelineOutput", RGResourceType::Texture, RGResourceLifetime::Transient);
        res.texture = &tex;
        RenderGraphResource* resources[] = { &res };
        const std::span<RenderGraphResource* const> span(resources, 1);
        solver.reset(span);

        RenderGraphPass writePass(u8"ForwardOpaque", RGPassType::Render);
        writePass.accesses.push_back(access(0, AT::WriteColorTarget));
        solver.emitBarriers(writePass, span, encoder);

        REQUIRE(encoder.textureBarriers.size() == 1u);
        CHECK(encoder.textureBarriers[0].oldState == RS::Undefined);
        CHECK(encoder.textureBarriers[0].newState == RS::RenderTarget);
    }
}
