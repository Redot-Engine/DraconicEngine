/// Procedural primitive meshes (`:primitives` partition).
///
/// Procedural primitive meshes (debug shapes / placeholders / tests). Each returns a
/// fully-formed StaticMesh - static vertex stream + 32-bit indices + one submesh +
/// generated tangents + bounds. These build the typed StaticMesh directly (no generic
/// untyped vertex-buffer indirection).

module;

#include <memory>

export module geometry:primitives;

import core;
import :types;
import :mesh;

using namespace draco;

export namespace draco::geometry {

class Primitives {
public:
    // A unit quad in the XY plane (two triangles), facing +Z.
    [[nodiscard]] static std::unique_ptr<StaticMesh> quad(f32 width = 1.0f, f32 height = 1.0f) {
        std::unique_ptr<StaticMesh> mesh = std::make_unique<StaticMesh>();
        const f32 hw = width * 0.5f, hh = height * 0.5f;
        const u32 white = 0xFFFFFFFFu;
        mesh->vertices.push_back(StaticMeshVertex{ math::Vector3{ -hw, -hh, 0 }, math::Vector3{ 0, 0, 1 }, math::Vector2{ 0, 1 }, white, math::Vector3{ 1, 0, 0 } });
        mesh->vertices.push_back(StaticMeshVertex{ math::Vector3{  hw, -hh, 0 }, math::Vector3{ 0, 0, 1 }, math::Vector2{ 1, 1 }, white, math::Vector3{ 1, 0, 0 } });
        mesh->vertices.push_back(StaticMeshVertex{ math::Vector3{  hw,  hh, 0 }, math::Vector3{ 0, 0, 1 }, math::Vector2{ 1, 0 }, white, math::Vector3{ 1, 0, 0 } });
        mesh->vertices.push_back(StaticMeshVertex{ math::Vector3{ -hw,  hh, 0 }, math::Vector3{ 0, 0, 1 }, math::Vector2{ 0, 0 }, white, math::Vector3{ 1, 0, 0 } });
        const u32 quad[6] = { 0, 1, 2, 0, 2, 3 };
        mesh->indices.resize(6);
        for (u32 i : quad) { mesh->indices.add(i); }
        finish(*mesh);
        return mesh;
    }

    // An axis-aligned cube of edge `size`, 24 verts (hard per-face normals).
    [[nodiscard]] static std::unique_ptr<StaticMesh> cube(f32 size = 1.0f) {
        std::unique_ptr<StaticMesh> mesh = std::make_unique<StaticMesh>();
        const f32 h = size * 0.5f;
        mesh->indices.resize(36);   // 6 faces x 2 triangles x 3 indices
        // 6 faces: (origin corner, edge-u, edge-v, normal)
        addFace(*mesh, math::Vector3{ -h, -h,  h }, math::Vector3{ 1, 0, 0 }, math::Vector3{ 0, 1, 0 }, math::Vector3{ 0, 0, 1 }, size);   // +Z
        addFace(*mesh, math::Vector3{  h, -h, -h }, math::Vector3{ -1, 0, 0 }, math::Vector3{ 0, 1, 0 }, math::Vector3{ 0, 0, -1 }, size);  // -Z
        addFace(*mesh, math::Vector3{  h, -h,  h }, math::Vector3{ 0, 0, -1 }, math::Vector3{ 0, 1, 0 }, math::Vector3{ 1, 0, 0 }, size);   // +X
        addFace(*mesh, math::Vector3{ -h, -h, -h }, math::Vector3{ 0, 0, 1 }, math::Vector3{ 0, 1, 0 }, math::Vector3{ -1, 0, 0 }, size);   // -X
        addFace(*mesh, math::Vector3{ -h,  h,  h }, math::Vector3{ 1, 0, 0 }, math::Vector3{ 0, 0, -1 }, math::Vector3{ 0, 1, 0 }, size);   // +Y
        addFace(*mesh, math::Vector3{ -h, -h, -h }, math::Vector3{ 1, 0, 0 }, math::Vector3{ 0, 0, 1 }, math::Vector3{ 0, -1, 0 }, size);   // -Y
        finish(*mesh);
        return mesh;
    }

    // A flat grid in the XZ plane, `width` x `depth`, subdivided.
    [[nodiscard]] static std::unique_ptr<StaticMesh> plane(f32 width = 1.0f, f32 depth = 1.0f, u32 xSegments = 1, u32 zSegments = 1) {
        std::unique_ptr<StaticMesh> mesh = std::make_unique<StaticMesh>();
        const u32 xs = xSegments < 1 ? 1 : xSegments, zs = zSegments < 1 ? 1 : zSegments;
        const u32 white = 0xFFFFFFFFu;
        for (u32 z = 0; z <= zs; ++z) {
            for (u32 x = 0; x <= xs; ++x) {
                const f32 u = static_cast<f32>(x) / static_cast<f32>(xs);
                const f32 v = static_cast<f32>(z) / static_cast<f32>(zs);
                mesh->vertices.push_back(StaticMeshVertex{
                    math::Vector3{ (u - 0.5f) * width, 0.0f, (v - 0.5f) * depth }, math::Vector3{ 0, 1, 0 }, math::Vector2{ u, v }, white, math::Vector3{ 1, 0, 0 } });
            }
        }
        mesh->indices.resize(xs * zs * 6);
        const u32 rowStride = xs + 1;
        for (u32 z = 0; z < zs; ++z) {
            for (u32 x = 0; x < xs; ++x) {
                const u32 i0 = z * rowStride + x, i1 = i0 + 1, i2 = i0 + rowStride, i3 = i2 + 1;
                mesh->indices.addTriangle(i0, i2, i1);
                mesh->indices.addTriangle(i1, i2, i3);
            }
        }
        finish(*mesh);
        return mesh;
    }

    // A UV sphere of `radius` with `segments` longitudes and `rings` latitudes.
    [[nodiscard]] static std::unique_ptr<StaticMesh> sphere(f32 radius = 0.5f, u32 segments = 32, u32 rings = 16) {
        std::unique_ptr<StaticMesh> mesh = std::make_unique<StaticMesh>();
        const u32 seg = segments < 3 ? 3 : segments, rng = rings < 2 ? 2 : rings;
        const u32 white = 0xFFFFFFFFu;
        for (u32 r = 0; r <= rng; ++r) {
            const f32 v = static_cast<f32>(r) / static_cast<f32>(rng);
            const f32 phi = v * math::PI;                       // 0..pi (pole to pole)
            const f32 sinPhi = math::sin(phi), cosPhi = math::cos(phi);
            for (u32 s = 0; s <= seg; ++s) {
                const f32 u = static_cast<f32>(s) / static_cast<f32>(seg);
                const f32 theta = u * 2.0f * math::PI;
                const math::Vector3 n{ math::cos(theta) * sinPhi, cosPhi, math::sin(theta) * sinPhi };
                mesh->vertices.push_back(StaticMeshVertex{ n * radius, n, math::Vector2{ u, v }, white, math::Vector3{ 1, 0, 0 } });
            }
        }
        mesh->indices.resize(seg * rng * 6);
        const u32 rowStride = seg + 1;
        for (u32 r = 0; r < rng; ++r) {
            for (u32 s = 0; s < seg; ++s) {
                const u32 i0 = r * rowStride + s, i1 = i0 + 1, i2 = i0 + rowStride, i3 = i2 + 1;
                // CCW-from-outside winding: (a,b,c),(b,d,c) with a=i0,b=i1,c=i2,d=i3.
                // Using (i0,i2,i1)/(i1,i2,i3) - last two swapped - reverses the front face
                // to inward, so back-face culling would hide the outer shell.
                mesh->indices.addTriangle(i0, i1, i2);
                mesh->indices.addTriangle(i1, i3, i2);
            }
        }
        finish(*mesh);
        return mesh;
    }

private:
    // Adds a quad face (4 verts, 2 tris) anchored at `origin`, spanning `size` along
    // unit edges `eu`/`ev`, with face normal `n`. The caller pre-sizes the index
    // buffer; indices append through its cursor.
    static void addFace(StaticMesh& mesh, math::Vector3 origin, math::Vector3 eu, math::Vector3 ev, math::Vector3 n, f32 size) {
        const u32 base = mesh.vertexCount();
        const u32 white = 0xFFFFFFFFu;
        const math::Vector3 u = eu * size, v = ev * size;
        mesh.vertices.push_back(StaticMeshVertex{ origin,             n, math::Vector2{ 0, 1 }, white, math::Vector3{ 1, 0, 0 } });
        mesh.vertices.push_back(StaticMeshVertex{ origin + u,         n, math::Vector2{ 1, 1 }, white, math::Vector3{ 1, 0, 0 } });
        mesh.vertices.push_back(StaticMeshVertex{ origin + u + v,     n, math::Vector2{ 1, 0 }, white, math::Vector3{ 1, 0, 0 } });
        mesh.vertices.push_back(StaticMeshVertex{ origin + v,         n, math::Vector2{ 0, 0 }, white, math::Vector3{ 1, 0, 0 } });
        mesh.indices.addTriangle(base, base + 1, base + 2);
        mesh.indices.addTriangle(base, base + 2, base + 3);
    }

    static void finish(StaticMesh& mesh) {
        mesh.generateTangents();
        mesh.calculateBounds();
        mesh.subMeshes.push_back(SubMesh{ 0, static_cast<i32>(mesh.indexCount()), 0, PrimitiveType::Triangles });
    }
};

} // namespace draco::geometry
