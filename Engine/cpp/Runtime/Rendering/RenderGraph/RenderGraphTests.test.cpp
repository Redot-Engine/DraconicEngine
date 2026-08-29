// Direct unit tests for pieces without a dedicated Sedulous test file
// (SubresourceStateTracker, PersistentResource ping-pong, resource tracking).
// The Sedulous suite is ported in Type/Descriptor/PassBuilder/Dependency/
// Culling/GraphCore/Barrier Tests.

#include <doctest_with_main.h>


import core;
import rhi;
import rendergraph;

using namespace draco;
using namespace draco::rendergraph;
namespace rhi = draco::rhi;

TEST_CASE("rg.persistent: ping-pong swap")
{
    auto* a = reinterpret_cast<rhi::Texture*>(0x10);
    auto* b = reinterpret_cast<rhi::Texture*>(0x20);
    auto* va = reinterpret_cast<rhi::TextureView*>(0x30);
    auto* vb = reinterpret_cast<rhi::TextureView*>(0x40);

    PersistentResource single(a, va);
    CHECK_FALSE(single.isPingPong());
    CHECK(single.currentTexture() == a);
    CHECK(single.previousTexture() == a);
    single.swap();
    CHECK(single.currentTexture() == a);

    PersistentResource pp(a, b, va, vb);
    CHECK(pp.isPingPong());
    CHECK(pp.currentTexture() == a);
    CHECK(pp.previousTexture() == b);
    pp.swap();
    CHECK(pp.currentTexture() == b);
    CHECK(pp.previousTexture() == a);
}

TEST_CASE("rg.resource: tracking + totals from descriptor")
{
    RenderGraphResource res(u8"gbuffer", RGResourceType::Texture, RGResourceLifetime::Transient);
    res.textureDesc.mipLevelCount = 4;
    res.textureDesc.arrayLayerCount = 6;

    CHECK(res.totalMipLevels() == 4u);     // no GPU texture -> from descriptor
    CHECK(res.totalArrayLayers() == 6u);

    res.refCount = 3;
    res.firstUsePass = 2;
    res.resetTracking();
    CHECK(res.refCount == 0);
    CHECK(res.firstUsePass == -1);
    CHECK_FALSE(res.firstWriter.isValid());
    CHECK_FALSE(res.finalState.has_value());
}

TEST_CASE("rg.state_tracker: uniform fast path, divergence, collapse")
{
    using RS = rhi::ResourceState;
    SubresourceStateTracker t(4, 2, RS::Undefined);
    CHECK(t.isUniform());
    CHECK(t.getState(0, 0) == RS::Undefined);

    t.setState(RGSubresourceRange::all(), RS::ShaderRead);
    CHECK(t.isUniform());
    CHECK(t.getState(3, 1) == RS::ShaderRead);

    t.setState(0, 1, 0, 1, RS::RenderTarget);
    CHECK_FALSE(t.isUniform());
    CHECK(t.getState(0, 0) == RS::RenderTarget);
    CHECK(t.getState(1, 0) == RS::ShaderRead);

    std::vector<RS> snapshot = t.copyStates();
    CHECK(snapshot.size() == 8u);

    t.setAll(RS::ShaderRead);
    CHECK(t.isUniform());

    SubresourceStateTracker restored(4, 2, RS::Undefined);
    restored.initFromStates(snapshot, RS::Undefined);
    CHECK_FALSE(restored.isUniform());
    CHECK(restored.getState(0, 0) == RS::RenderTarget);

    restored.setState(0, 1, 0, 1, RS::ShaderRead);
    CHECK(restored.isUniform());
}
