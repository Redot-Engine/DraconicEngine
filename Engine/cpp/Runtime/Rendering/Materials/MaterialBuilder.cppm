/// Fluent builder for authoring a Material in code (`:builder` partition).
///
/// Fluent builder for authoring a Material in code: declare the shader, pipeline
/// state, and typed properties; the builder lays out the uniform buffer (std140-ish
/// 16-byte alignment for float3/float4) and assigns binding ordinals. build() hands
/// back the finished Material (a shared_ptr).

module;
#include <string>
#include <string_view>
#include <memory>

export module materials:builder;

import core;
import rhi;
import shaders;
import :types;
import :pipeline;
import :material;

using namespace draco;
namespace rhi = draco::rhi;

export namespace draco::materials {

class MaterialBuilder {
public:
    explicit MaterialBuilder(std::u8string_view name) {
        m_material = std::make_shared<Material>();
        m_material->name = std::u8string(name);
        m_material->pipeline = PipelineConfig{};
    }

    MaterialBuilder& shader(std::u8string_view shaderName) {
        m_material->shaderName = std::u8string(shaderName);
        m_material->pipeline.shaderName = m_material->shaderName;
        return *this;
    }
    MaterialBuilder& flags(shaders::ShaderFlags flags) {
        m_material->shaderFlags = flags;
        m_material->pipeline.shaderFlags = flags;
        return *this;
    }
    MaterialBuilder& vertexLayout(VertexLayoutType layout) { m_material->pipeline.vertexLayout = layout; return *this; }
    MaterialBuilder& blend(BlendMode mode)                 { m_material->pipeline.blendMode = mode; return *this; }
    MaterialBuilder& depth(DepthMode mode)                 { m_material->pipeline.depthMode = mode; return *this; }
    MaterialBuilder& cull(CullModeConfig mode)             { m_material->pipeline.cullMode = mode; return *this; }
    MaterialBuilder& doubleSided()                         { m_material->pipeline.cullMode = CullModeConfig::None; return *this; }
    MaterialBuilder& transparent() {
        m_material->pipeline.blendMode = BlendMode::AlphaBlend;
        m_material->pipeline.depthMode = DepthMode::ReadOnly;
        return *this;
    }
    MaterialBuilder& additive() {
        m_material->pipeline.blendMode = BlendMode::Additive;
        m_material->pipeline.depthMode = DepthMode::ReadOnly;
        return *this;
    }

    // --- uniform properties (laid out into the material uniform buffer) ---
    MaterialBuilder& Float(std::u8string_view name, f32 v = 0.0f) {
        addUniform(name, MaterialPropertyType::Float, 4, /*align16*/ false);
        m_material->allocateDefaultUniformData();
        m_material->setDefaultFloat(name, v);
        return *this;
    }
    MaterialBuilder& Float2(std::u8string_view name, math::Vector2 v = {}) {
        addUniform(name, MaterialPropertyType::Float2, 8, false);
        m_material->allocateDefaultUniformData();
        m_material->setDefaultFloat2(name, v);
        return *this;
    }
    MaterialBuilder& Float3(std::u8string_view name, math::Vector3 v = {}) {
        addUniform(name, MaterialPropertyType::Float3, 12, /*align16*/ true);   // float3 occupies 16 (std140)
        m_material->allocateDefaultUniformData();
        m_material->setDefaultFloat3(name, v);
        return *this;
    }
    MaterialBuilder& Float4(std::u8string_view name, math::Vector4 v = {}) {
        addUniform(name, MaterialPropertyType::Float4, 16, true);
        m_material->allocateDefaultUniformData();
        m_material->setDefaultFloat4(name, v);
        return *this;
    }
    MaterialBuilder& Color(std::u8string_view name, math::Vector4 v = math::Vector4{ 1, 1, 1, 1 }) { return Float4(name, v); }

    // --- resource properties (become bind-group entries) ---
    MaterialBuilder& texture(std::u8string_view name, rhi::TextureView* def = nullptr) {
        m_material->addProperty(MaterialPropertyDef{ name, MaterialPropertyType::Texture2D, m_binding++, 0, 0 });
        if (def != nullptr) { m_material->setDefaultTexture(name, def); }
        return *this;
    }
    MaterialBuilder& textureCube(std::u8string_view name, rhi::TextureView* def = nullptr) {
        m_material->addProperty(MaterialPropertyDef{ name, MaterialPropertyType::TextureCube, m_binding++, 0, 0 });
        if (def != nullptr) { m_material->setDefaultTexture(name, def); }
        return *this;
    }
    MaterialBuilder& sampler(std::u8string_view name, rhi::Sampler* def = nullptr) {
        m_material->addProperty(MaterialPropertyDef{ name, MaterialPropertyType::Sampler, m_binding++, 0, 0 });
        if (def != nullptr) { m_material->setDefaultSampler(name, def); }
        return *this;
    }

    // Finishes and yields the material. The builder is empty afterwards.
    [[nodiscard]] std::shared_ptr<Material> build() {
        m_material->allocateDefaultUniformData();
        return std::move(m_material);
    }

private:
    void addUniform(std::u8string_view name, MaterialPropertyType type, u32 size, bool align16) {
        if (align16) { m_uniformOffset = (m_uniformOffset + 15u) & ~15u; }
        m_material->addProperty(MaterialPropertyDef{ name, type, m_binding, m_uniformOffset, size });
        m_uniformOffset += align16 ? 16u : size;   // float3/float4 consume a 16-byte slot
        ++m_binding;
    }

    std::shared_ptr<Material> m_material;
    u32 m_uniformOffset = 0;
    u32 m_binding = 0;
};

// Standard PBR material: the BaseColor/Metallic/Roughness
// uniforms + the five PBR maps + a sampler, in the order the forward set-2 contract expects. The
// importer builds these and assigns the maps; unset maps fall back to neutral defaults in the renderer.
[[nodiscard]] inline std::shared_ptr<Material> createPBR(std::u8string_view name, math::Vector4 baseColor = math::Vector4{ 1, 1, 1, 1 },
                                               f32 metallic = 0.0f, f32 roughness = 0.5f,
                                               std::u8string_view shaderName = u8"forward") {
    return MaterialBuilder(name)
        .shader(shaderName)
        .vertexLayout(VertexLayoutType::Mesh)
        .Color(u8"BaseColor", baseColor)
        .Float(u8"Metallic", metallic)
        .Float(u8"Roughness", roughness)
        .texture(u8"AlbedoMap")
        .texture(u8"NormalMap")
        .texture(u8"MetallicRoughnessMap")
        .texture(u8"OcclusionMap")
        .texture(u8"EmissiveMap")
        .sampler(u8"MainSampler")
        .build();
}

} // namespace draco::materials
