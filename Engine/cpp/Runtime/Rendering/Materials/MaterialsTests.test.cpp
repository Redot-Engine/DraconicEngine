// The data-driven material model: build a material with the fluent builder, verify
// uniform-buffer layout + declared properties, drive a MaterialSystem (bind-group
// layout inferred from the property list) against the Null RHI, and check instance
// overrides + dirty notification.
#include <doctest_with_main.h>
#include <memory>
#include <span>

import core;
import rhi;
import rhi.null;
import shaders;
import materials;

using namespace draco;
using namespace draco::materials;
namespace rhi = draco::rhi;
namespace shaders = draco::shaders;

TEST_CASE("material builder: lays out uniforms + declares properties")
{
    std::shared_ptr<Material> mat = MaterialBuilder(u8"pbr")
        .shader(u8"forward")
        .flags(shaders::ShaderFlags::NormalMap)
        .Color(u8"baseColor", math::Vector4{ 1, 0, 0, 1 })   // float4 -> 16 bytes, offset 0
        .Float(u8"roughness", 0.5f)                  // float  ->  4 bytes, offset 16
        .texture(u8"albedoMap")
        .texture(u8"normalMap")
        .sampler(u8"linearSampler")
        .build();

    REQUIRE(mat);
    CHECK(mat->isValid());
    CHECK(mat->shaderName == u8"forward");
    CHECK(mat->shaderFlags == shaders::ShaderFlags::NormalMap);
    CHECK(mat->pipeline.shaderName == u8"forward");          // pipeline mirrors shader id
    CHECK(mat->propertyCount() == 5);
    CHECK(mat->uniformDataSize() == 20);                     // 16 (float4) + 4 (float)

    CHECK(mat->getPropertyIndex(u8"roughness") == 1);
    CHECK(mat->getPropertyIndex(u8"missing") == -1);
    const MaterialPropertyDef* rough = mat->findProperty(u8"roughness");
    REQUIRE(rough != nullptr);
    CHECK(rough->offset == 16);
    CHECK(rough->isUniform());

    // default uniform data carries the seeded baseColor (red) at offset 0
    const std::span<const u8> defaults = mat->defaultUniformData();
    REQUIRE(defaults.size() == 20);
    const f32* base = reinterpret_cast<const f32*>(defaults.data());
    CHECK(base[0] == doctest::Approx(1.0f));
    CHECK(base[1] == doctest::Approx(0.0f));
}

TEST_CASE("material system: infers bind-group layout from properties + builds instance bind group")
{
    rhi::null::NullDevice device;
    MaterialSystem system;
    REQUIRE(system.initialize(device).isOk());
    CHECK(system.defaultSampler() != nullptr);
    CHECK(system.whiteTexture() != nullptr);
    CHECK(system.normalTexture() != nullptr);

    std::shared_ptr<Material> mat = MaterialBuilder(u8"lit")
        .shader(u8"forward")
        .Color(u8"tint", math::Vector4{ 1, 1, 1, 1 })
        .texture(u8"albedoMap")
        .sampler(u8"samp")
        .build();
    REQUIRE(mat);

    // layout is cached: same material -> same pointer
    rhi::BindGroupLayout* l0 = system.getOrCreateLayout(*mat);
    rhi::BindGroupLayout* l1 = system.getOrCreateLayout(*mat);
    REQUIRE(l0 != nullptr);
    CHECK(l0 == l1);

    MaterialInstance inst(mat.get());
    rhi::BindGroup* bg = system.prepareInstance(inst);
    REQUIRE(bg != nullptr);
    CHECK(inst.bindGroupLayout() == l0);
    CHECK_FALSE(inst.isUniformDirty());
    CHECK_FALSE(inst.isBindGroupDirty());
    CHECK(system.getBindGroup(inst) == bg);
}

TEST_CASE("material instance: overrides notify the system + re-prep is driven by the dirty list")
{
    rhi::null::NullDevice device;
    MaterialSystem system;
    REQUIRE(system.initialize(device).isOk());

    std::shared_ptr<Material> mat = MaterialBuilder(u8"lit")
        .shader(u8"forward")
        .Float(u8"roughness", 0.5f)
        .texture(u8"albedoMap")
        .build();
    REQUIRE(mat);

    MaterialInstance inst(mat.get());
    REQUIRE(system.prepareInstance(inst) != nullptr);   // clears dirty + registers sink

    // overriding a uniform marks it dirty and enqueues exactly one dirty entry
    inst.setFloat(u8"roughness", 0.9f);
    CHECK(inst.isUniformDirty());
    inst.setFloat(u8"roughness", 0.8f);                 // second set: still one enqueue

    system.prepareDirtyInstances();
    CHECK_FALSE(inst.isUniformDirty());                 // drained + re-prepped

    // the override is the effective value (0.8), not the material default (0.5)
    const std::span<const u8> data = inst.uniformData();
    REQUIRE(data.size() == 4);
    CHECK(*reinterpret_cast<const f32*>(data.data()) == doctest::Approx(0.8f));

    // resetting restores the default
    inst.resetProperty(u8"roughness");
    system.prepareDirtyInstances();
    CHECK(*reinterpret_cast<const f32*>(inst.uniformData().data()) == doctest::Approx(0.5f));
}

TEST_CASE("pipeline config: content hash + equality distinguish render state")
{
    PipelineConfig a = PipelineConfig::forOpaqueMesh(u8"forward");
    PipelineConfig b = PipelineConfig::forOpaqueMesh(u8"forward");
    CHECK(a == b);
    CHECK(a.hashCode() == b.hashCode());

    PipelineConfig c = PipelineConfig::forTransparentMesh(u8"forward");
    CHECK_FALSE(a == c);                                 // blend/depth differ
    CHECK(a.hashCode() != c.hashCode());

    CHECK(VertexLayoutHelper::stride(VertexLayoutType::Mesh) == 48);
    CHECK(VertexLayoutHelper::attributes(VertexLayoutType::Mesh).size() == 5);
}
