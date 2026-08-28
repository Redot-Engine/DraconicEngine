/// Standard and skinned vertex structs for model data.

export module model:model_vertex;

import core;

using namespace draco;

export namespace draco::model {

/// Standard vertex format for static models (48 bytes).
struct ModelVertex {
    math::Float3 position{};             // 12 bytes
    math::Float3 normal{ 0, 1, 0 };      // 12 bytes
    math::Float2 texCoord{};             // 8 bytes
    u32  color = 0xFFFFFFFF;     // 4 bytes (packed RGBA)
    math::Float3 tangent{ 1, 0, 0 };     // 12 bytes
    // Total: 48 bytes

    constexpr ModelVertex() = default;
    constexpr ModelVertex(math::Vector3 pos, math::Vector3 nrm, math::Vector2 uv, u32 col = 0xFFFFFFFF, math::Vector3 tan = { 1, 0, 0 })
        : position(pos), normal(nrm), texCoord(uv), color(col), tangent(tan) {}
};

static_assert(sizeof(ModelVertex) == 48, "ModelVertex must be 48 bytes");

/// Skinned vertex format for animated models (72 bytes).
struct SkinnedModelVertex {
    math::Float3 position{};             // 12 bytes
    math::Float3 normal{ 0, 1, 0 };      // 12 bytes
    math::Float2 texCoord{};             // 8 bytes
    u32  color = 0xFFFFFFFF;     // 4 bytes (packed RGBA)
    math::Float3 tangent{ 1, 0, 0 };     // 12 bytes
    u16  joints[4] = { 0, 0, 0, 0 }; // 8 bytes (up to 4 bone indices)
    math::Float4 weights{ 1, 0, 0, 0 };       // 16 bytes (bone weights)
    // Total: 72 bytes

    constexpr SkinnedModelVertex() = default;
    constexpr SkinnedModelVertex(math::Vector3 pos, math::Vector3 nrm, math::Vector2 uv, u32 col, math::Vector3 tan,
                                 u16 j0, u16 j1, u16 j2, u16 j3, math::Vector4 wt)
        : position(pos), normal(nrm), texCoord(uv), color(col), tangent(tan),
          joints{ j0, j1, j2, j3 }, weights(wt) {}
};

static_assert(sizeof(SkinnedModelVertex) == 72, "SkinnedModelVertex must be 72 bytes");

} // namespace draco::model
