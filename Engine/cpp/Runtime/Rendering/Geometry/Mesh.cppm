/// StaticMesh / SkinnedMesh runtime mesh formats (`:mesh` partition).
///
/// StaticMesh is the engine's runtime mesh: a static vertex stream (48B), an index
/// buffer, submeshes, bounds, and the geometry ops (normals/tangents/bounds) that
/// operate purely on the static stream.
///
/// SkinnedMesh IS-A StaticMesh: it inherits the entire static stream + all the ops
/// and adds only a *parallel* skinning stream (joints/weights, 24B) + a skeleton
/// reference. Because the static data is byte-identical and in the same place, a
/// SkinnedMesh can be passed anywhere a StaticMesh& is expected - the static draw
/// path just works; the skinned path additionally binds the skinning stream. The
/// virtual isSkinned()/skinningStream() hooks let a consumer holding a StaticMesh&
/// discover + bind the skinning stream without RTTI.

module;

#include <span>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

export module geometry:mesh;

import core;
import :types;
import :index_buffer;

using namespace draco;

export namespace draco::geometry {

class StaticMesh {
public:
    virtual ~StaticMesh() = default;

    std::u8string                   name;
    std::vector<StaticMeshVertex>  vertices;
    IndexBuffer              indices{ IndexBuffer::Format::U32 };
    std::vector<SubMesh>           subMeshes;
    math::AABB                     bounds = math::AABB::empty();

    [[nodiscard]] u32 vertexCount() const noexcept { return static_cast<u32>(vertices.size()); }
    [[nodiscard]] u32 indexCount()  const noexcept { return indices.count(); }
    [[nodiscard]] static constexpr u32 vertexStride() noexcept { return sizeof(StaticMeshVertex); }

    // Raw static-stream bytes for GPU upload (null when empty).
    [[nodiscard]] const u8* vertexData() const noexcept {
        return vertices.empty() ? nullptr : reinterpret_cast<const u8*>(vertices.data());
    }
    [[nodiscard]] u32 vertexDataSize() const noexcept { return vertexCount() * vertexStride(); }

    // Substitutability hooks: a consumer with a StaticMesh& can ask whether it is
    // really skinned and bind the parallel stream, no RTTI needed.
    [[nodiscard]] virtual bool isSkinned() const noexcept { return false; }
    [[nodiscard]] virtual std::span<const VertexSkinning> skinningStream() const noexcept { return {}; }

    // Resets the mesh to empty in place, so a hot-reload can re-populate the same
    // instance without invalidating outside references (GPU caches, renderers).
    virtual void clearForReload() {
        name.clear();
        vertices.clear();
        indices.clear();
        subMeshes.clear();
        bounds = math::AABB::empty();
    }

    // Recomputes `bounds` from the static stream.
    math::AABB& calculateBounds() {
        if (vertices.empty()) { bounds = math::AABB{ math::Vector3::zero, math::Vector3::zero }; return bounds; }
        bounds = math::AABB::empty();
        for (const StaticMeshVertex& v : vertices) { bounds.expand(v.position); }
        return bounds;
    }

    // Smooth normals: accumulate per-triangle face normals into shared vertices.
    void generateNormals() {
        const u32 triangles = triangleCount();
        if (triangles == 0) { return; }
        for (StaticMeshVertex& v : vertices) { v.normal = math::Float3{ 0, 0, 0 }; }
        for (u32 t = 0; t < triangles; ++t) {
            const u32 i0 = corner(t, 0), i1 = corner(t, 1), i2 = corner(t, 2);
            const math::Float3 e1 = vertices[i1].position - vertices[i0].position;
            const math::Float3 e2 = vertices[i2].position - vertices[i0].position;
            const math::Float3 faceNormal = cross(e1, e2);
            vertices[i0].normal += faceNormal;
            vertices[i1].normal += faceNormal;
            vertices[i2].normal += faceNormal;
        }
        for (StaticMeshVertex& v : vertices) {
            v.normal = (lengthSquared(v.normal) > 0.0001f) ? normalize(v.normal) : math::Float3{ 0, 1, 0 };
        }
    }

    // Tangents for normal mapping: per-triangle (deltaUV-weighted) accumulation,
    // then Gram-Schmidt orthogonalization against the normal.
    void generateTangents() {
        const u32 triangles = triangleCount();
        if (triangles == 0) { return; }
        for (StaticMeshVertex& v : vertices) { v.tangent = math::Float3{ 0, 0, 0 }; }
        for (u32 t = 0; t < triangles; ++t) {
            const u32 i0 = corner(t, 0), i1 = corner(t, 1), i2 = corner(t, 2);
            const math::Float3 dp1 = vertices[i1].position - vertices[i0].position;
            const math::Float3 dp2 = vertices[i2].position - vertices[i0].position;
            const math::Float2 du1 = vertices[i1].texCoord - vertices[i0].texCoord;
            const math::Float2 du2 = vertices[i2].texCoord - vertices[i0].texCoord;
            const f32 denom = du1.x * du2.y - du2.x * du1.y;
            math::Float3 tangent = math::Float3{ 0, 0, 0 };
            if (std::abs(denom) > 0.0001f) {
                const f32 r = 1.0f / denom;
                tangent = (dp1 * du2.y - dp2 * du1.y) * r;
            }
            vertices[i0].tangent += tangent;
            vertices[i1].tangent += tangent;
            vertices[i2].tangent += tangent;
        }
        for (StaticMeshVertex& v : vertices) {
            if (lengthSquared(v.tangent) > 0.0001f) {
                v.tangent = v.tangent - v.normal * dot(v.normal, v.tangent);   // orthogonalize
                v.tangent = (lengthSquared(v.tangent) > 0.0001f) ? normalize(v.tangent) : defaultTangent(v.normal);
            } else {
                v.tangent = defaultTangent(v.normal);
            }
        }
    }

    // Pack a 0..1 float color to RGBA8 with R in the low byte (Unorm8x4 order).
    [[nodiscard]] static u32 packColor(math::Vector4 c) {
        const u32 r = static_cast<u32>(std::clamp(c.x, 0.0f, 1.0f) * 255.0f);
        const u32 g = static_cast<u32>(std::clamp(c.y, 0.0f, 1.0f) * 255.0f);
        const u32 b = static_cast<u32>(std::clamp(c.z, 0.0f, 1.0f) * 255.0f);
        const u32 a = static_cast<u32>(std::clamp(c.w, 0.0f, 1.0f) * 255.0f);
        return r | (g << 8) | (b << 16) | (a << 24);
    }
    [[nodiscard]] static u32 packColor(Color32 c) {
        return static_cast<u32>(c.r) | (static_cast<u32>(c.g) << 8)
             | (static_cast<u32>(c.b) << 16) | (static_cast<u32>(c.a) << 24);
    }

protected:
    [[nodiscard]] u32 triangleCount() const noexcept {
        const bool hasIndices = indices.count() > 0;
        return hasIndices ? indices.count() / 3u : vertexCount() / 3u;
    }
    // Vertex index of corner `c` (0..2) of triangle `t` (indexed or sequential).
    [[nodiscard]] u32 corner(u32 t, u32 c) const noexcept {
        return (indices.count() > 0) ? indices.get(t * 3 + c) : (t * 3 + c);
    }
    [[nodiscard]] static math::Float3 defaultTangent(math::Float3 normal) {
        const math::Float3 t = (std::abs(normal.y) < 0.9f) ? cross(normal, math::Float3{ 0, 1, 0 }) : cross(normal, math::Float3{ 1, 0, 0 });
        return (lengthSquared(t) > 0.0001f) ? normalize(t) : math::Float3{ 1, 0, 0 };
    }
};

// A skinned mesh: the static stream + a parallel skinning stream + a skeleton ref.
class SkinnedMesh final : public StaticMesh {
public:
    std::vector<VertexSkinning> skinning;            // parallel to StaticMesh::vertices
    i32                   skeletonIndex = -1;   // index into the import skeleton list (-1 = none)

    [[nodiscard]] bool isSkinned() const noexcept override { return true; }
    [[nodiscard]] std::span<const VertexSkinning> skinningStream() const noexcept override {
        return { skinning.data(), skinning.size() };
    }

    [[nodiscard]] static constexpr u32 skinningStride() noexcept { return sizeof(VertexSkinning); }
    [[nodiscard]] const u8* skinningData() const noexcept {
        return skinning.empty() ? nullptr : reinterpret_cast<const u8*>(skinning.data());
    }
    [[nodiscard]] u32 skinningDataSize() const noexcept { return static_cast<u32>(skinning.size()) * skinningStride(); }

    void clearForReload() override {
        StaticMesh::clearForReload();
        skinning.clear();
        skeletonIndex = -1;
    }
};

// Safe downcast to SkinnedMesh via the virtual isSkinned() tag - no RTTI. SkinnedMesh
// is the sole StaticMesh subclass, so an isSkinned() StaticMesh is always a SkinnedMesh.
[[nodiscard]] inline SkinnedMesh* toSkinned(StaticMesh* m) noexcept {
    return (m && m->isSkinned()) ? static_cast<SkinnedMesh*>(m) : nullptr;
}
[[nodiscard]] inline const SkinnedMesh* toSkinned(const StaticMesh* m) noexcept {
    return (m && m->isSkinned()) ? static_cast<const SkinnedMesh*>(m) : nullptr;
}

} // namespace draco::geometry
