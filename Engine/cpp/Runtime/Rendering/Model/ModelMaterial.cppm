/// Material properties for PBR rendering.

module;
#include <string_view>

#include <string>

export module model:model_material;

import core;

using namespace draco;

export namespace draco::model {

/// Alpha blending mode.
enum class AlphaMode : u32 {
    Opaque,
    Mask,
    Blend,
};

/// PBR material properties for a model.
class ModelMaterial {
public:
    ModelMaterial() = default;
    ~ModelMaterial() = default;

    [[nodiscard]] std::u8string_view name() const { return std::u8string_view(m_name.data(), m_name.size()); }
    void setName(std::u8string_view n) { m_name = std::u8string(n); }

    // -- Base color --
    math::Vector4 baseColorFactor{ 1, 1, 1, 1 };
    i32  baseColorTextureIndex = -1;

    // -- Metallic-Roughness --
    f32 metallicFactor  = 1.0f;
    f32 roughnessFactor = 1.0f;
    i32 metallicRoughnessTextureIndex = -1;

    // -- Normal map --
    f32 normalScale = 1.0f;
    i32 normalTextureIndex = -1;

    // -- Occlusion --
    f32 occlusionStrength = 1.0f;
    i32 occlusionTextureIndex = -1;

    // -- Emissive --
    math::Vector3 emissiveFactor{};
    i32  emissiveTextureIndex = -1;

    // -- Alpha --
    AlphaMode alphaMode = AlphaMode::Opaque;
    f32 alphaCutoff = 0.5f;

    // -- Double-sided --
    bool doubleSided = false;

private:
    std::u8string m_name;
};

} // namespace draco::model
