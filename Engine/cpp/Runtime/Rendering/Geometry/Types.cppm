/// Value-type vocabulary for the runtime mesh format (`:types` partition).
///
/// Value-type vocabulary for the engine's runtime mesh format (distinct from
/// draco.model, which is the importer's representation of a loaded file): the
/// primitive topology, a submesh range, and the two vertex streams. A skinned mesh
/// reuses the static vertex stream and adds a *parallel* skinning stream, so its
/// static data is byte-identical to a static mesh (see :mesh).

module;

export module geometry:types;

import core;

using namespace draco;

export namespace draco::geometry {

// Primitive topology a submesh is drawn with.
enum class PrimitiveType : u8 {
    Triangles, TriangleStrip, TriangleFan, Lines, LineStrip, Points,
};

// A contiguous index range with its own material + topology.
struct SubMesh {
    i32           startIndex    = 0;
    i32           indexCount    = 0;
    i32           materialIndex = 0;
    PrimitiveType primitiveType = PrimitiveType::Triangles;
};

// The static vertex stream - 48 bytes, matching VertexLayoutType::Mesh (locations
// 0..4). Uses core's packed math::FloatN so the stream stays byte-tight for direct GPU
// upload (the aligned VectorN would pad it past 48 bytes).
struct StaticMeshVertex {
    math::Float3 position{ 0, 0, 0 };   // 12
    math::Float3 normal{ 0, 1, 0 };     // 12
    math::Float2 texCoord{ 0, 0 };      //  8
    u32          color = 0xFFFFFFFFu;   //  4  packed RGBA, R in the low byte (Unorm8x4)
    math::Float3 tangent{ 1, 0, 0 };    // 12
    // total: 48

    constexpr StaticMeshVertex() noexcept = default;
    constexpr StaticMeshVertex(math::Vector3 pos, math::Vector3 nrm, math::Vector2 uv, u32 col, math::Vector3 tan) noexcept
        : position(pos), normal(nrm), texCoord(uv), color(col), tangent(tan) {}
};

// The parallel skinning stream - 24 bytes (locations 6/7). One per static vertex; a
// skinned mesh stores this alongside the inherited static stream rather than
// interleaving, so the static stream stays substitutable for a static mesh.
struct VertexSkinning {
    u16          joints[4] = { 0, 0, 0, 0 };   //  8  bone indices (uint16x4, packed uint32x2)
    math::Float4 weights{ 1, 0, 0, 0 };        // 16  bone weights (sum to 1)
    // total: 24
};

static_assert(sizeof(StaticMeshVertex) == 48, "static vertex must stay 48 bytes (VertexLayoutType::Mesh)");
static_assert(sizeof(VertexSkinning) == 24, "skinning stream must stay 24 bytes (locations 6/7)");

} // namespace draco::geometry
