// The render-side PSO cache: build a pipeline from a PipelineConfig pulling variants
// from the ShaderSystem, verify it caches, and verify the version-polling hot-reload
// path - invalidating the shader makes GetPipeline rebuild and retire the stale PSO.
// Real DXC + Null RHI.
#include <doctest_with_main.h>
#include <memory>
#include <span>

import core;
import rhi;
import rhi.null;
import shaders;
import shaders.system;
import materials;
import materials.pso;

using namespace draco;
using namespace draco::materials;
namespace rhi = draco::rhi;
namespace shaders = draco::shaders;

namespace
{
    constexpr const char8_t* kVtx = u8"float4 main(uint id : SV_VertexID) : SV_Position { return float4(0,0,0,1); }\n";
    constexpr const char8_t* kFrag = u8"float4 main() : SV_Target { return float4(1,0,0,1); }\n";
}

TEST_CASE("pso cache: builds + caches a pipeline; shader reload rebuilds via version poll")
{
    shaders::Compiler* compiler = nullptr;
    if (!shaders::createCompiler(shaders::CompilerDesc{}, compiler).isOk()) { MESSAGE("DXC unavailable; skipping"); return; }

    rhi::null::NullDevice device;
    shaders::ShaderSystem shaderSystem(*compiler, device);
    shaderSystem.registerSource(u8"forward", shaders::ShaderStage::Vertex,   kVtx);
    shaderSystem.registerSource(u8"forward", shaders::ShaderStage::Fragment, kFrag);

    rhi::PipelineLayout* layout = nullptr;
    REQUIRE(device.createPipelineLayout(rhi::PipelineLayoutDesc{}, layout).isOk());

    PipelineStateCache cache(shaderSystem, device);
    const PipelineConfig config = PipelineConfig::forOpaqueMesh(u8"forward");

    rhi::RenderPipeline* p0 = cache.getPipeline(config, layout);
    REQUIRE(p0 != nullptr);
    CHECK(cache.size() == 1);

    // same request -> same pipeline (cache hit, no rebuild)
    rhi::RenderPipeline* p1 = cache.getPipeline(config, layout);
    CHECK(p1 == p0);
    CHECK(cache.size() == 1);
    CHECK(cache.retiredCount() == 0);

    // reload the shader: version bumps -> next GetPipeline rebuilds + retires the old PSO
    shaderSystem.invalidateShader(u8"forward");
    rhi::RenderPipeline* p2 = cache.getPipeline(config, layout);
    REQUIRE(p2 != nullptr);
    CHECK(p2 != p0);                 // rebuilt against the new shader
    CHECK(cache.size() == 1);        // same key, replaced in place
    CHECK(cache.retiredCount() == 1); // stale pipeline retired, awaiting GPU-safe free

    cache.releaseRetired();
    CHECK(cache.retiredCount() == 0);

    // a distinct render state is a distinct cache entry
    rhi::RenderPipeline* pT = cache.getPipeline(PipelineConfig::forTransparentMesh(u8"forward"), layout);
    REQUIRE(pT != nullptr);
    CHECK(cache.size() == 2);

    cache.clear();
    CHECK(cache.size() == 0);

    device.destroyPipelineLayout(layout);
    compiler->destroy();
}

TEST_CASE("pso cache: depth-only config builds without a fragment shader")
{
    shaders::Compiler* compiler = nullptr;
    if (!shaders::createCompiler(shaders::CompilerDesc{}, compiler).isOk()) { MESSAGE("DXC unavailable; skipping"); return; }

    rhi::null::NullDevice device;
    shaders::ShaderSystem shaderSystem(*compiler, device);
    shaderSystem.registerSource(u8"shadow", shaders::ShaderStage::Vertex, kVtx);
    // deliberately no fragment source registered

    rhi::PipelineLayout* layout = nullptr;
    REQUIRE(device.createPipelineLayout(rhi::PipelineLayoutDesc{}, layout).isOk());

    PipelineStateCache cache(shaderSystem, device);
    PipelineConfig config = PipelineConfig::forOpaqueMesh(u8"shadow");
    config.depthOnly = true;
    config.vertexLayout = VertexLayoutType::PositionOnly;

    rhi::RenderPipeline* p = cache.getPipeline(config, layout);
    CHECK(p != nullptr);             // vertex-only pipeline, no fragment fetched

    cache.clear();
    device.destroyPipelineLayout(layout);
    compiler->destroy();
}
