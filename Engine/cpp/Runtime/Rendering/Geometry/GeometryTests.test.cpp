// The engine runtime mesh format: vertex/stream sizes, the index buffer, StaticMesh
// geometry ops, and the key design point -- SkinnedMesh IS-A StaticMesh, so its static
// stream is usable anywhere a StaticMesh is, with the skinning stream discoverable via
// the virtual hooks. Plus the procedural primitives.
#include <doctest_with_main.h>
#include <memory>

import core;
import geometry;

using namespace draco;
using namespace draco::geometry;

TEST_CASE("stream layouts are the GPU-canonical sizes")
{
    CHECK(sizeof(StaticMeshVertex) == 48);
    CHECK(sizeof(VertexSkinning) == 24);
    CHECK(StaticMesh::vertexStride() == 48);
    CHECK(SkinnedMesh::skinningStride() == 24);
}

TEST_CASE("index buffer: format, set/get, raw size")
{
    IndexBuffer ib(IndexBuffer::Format::U16);
    CHECK(ib.indexSize() == 2);
    ib.resize(3);
    ib.addTriangle(0, 1, 2);
    CHECK(ib.count() == 3);
    CHECK(ib.get(0) == 0);
    CHECK(ib.get(2) == 2);
    CHECK(ib.dataSize() == 6);
    CHECK(ib.rawData() != nullptr);

    IndexBuffer ib32(IndexBuffer::Format::U32);
    CHECK(ib32.indexSize() == 4);
    ib32.resize(2);
    ib32.set(0, 70000);                 // exceeds u16 range -> needs 32-bit
    CHECK(ib32.get(0) == 70000);
}

TEST_CASE("static mesh: generated normals + tangents + bounds on a quad")
{
    std::unique_ptr<StaticMesh> mesh = Primitives::quad(2.0f, 2.0f);
    REQUIRE(mesh);
    CHECK(mesh->vertexCount() == 4);
    CHECK(mesh->indexCount() == 6);
    CHECK(mesh->subMeshes.size() == 1);

    mesh->generateNormals();
    for (const StaticMeshVertex& v : mesh->vertices) {
        CHECK(v.normal.z == doctest::Approx(1.0f));      // quad faces +Z
        CHECK(lengthSquared(v.tangent) == doctest::Approx(1.0f));   // unit tangents
    }

    mesh->calculateBounds();
    CHECK(mesh->bounds.min.x == doctest::Approx(-1.0f));
    CHECK(mesh->bounds.max.y == doctest::Approx(1.0f));
    CHECK(mesh->vertexDataSize() == 4 * 48);
    CHECK(mesh->vertexData() != nullptr);
}

TEST_CASE("skinned mesh IS-A static mesh: static stream is substitutable")
{
    std::unique_ptr<SkinnedMesh> skinned = std::make_unique<SkinnedMesh>();
    skinned->skeletonIndex = 3;
    // static stream (inherited)
    skinned->vertices.push_back(StaticMeshVertex{ math::Vector3{ 0, 0, 0 }, math::Vector3{ 0, 1, 0 }, math::Vector2{ 0, 0 }, 0xFFFFFFFFu, math::Vector3{ 1, 0, 0 } });
    skinned->vertices.push_back(StaticMeshVertex{ math::Vector3{ 1, 0, 0 }, math::Vector3{ 0, 1, 0 }, math::Vector2{ 1, 0 }, 0xFFFFFFFFu, math::Vector3{ 1, 0, 0 } });
    skinned->vertices.push_back(StaticMeshVertex{ math::Vector3{ 0, 1, 0 }, math::Vector3{ 0, 1, 0 }, math::Vector2{ 0, 1 }, 0xFFFFFFFFu, math::Vector3{ 1, 0, 0 } });
    // parallel skinning stream
    VertexSkinning s{}; s.joints[0] = 2; s.weights = math::Vector4{ 1, 0, 0, 0 };
    for (u32 i = 0; i < 3; ++i) { skinned->skinning.push_back(s); }

    // pass it where a StaticMesh& is expected -- the static ops just work
    StaticMesh& asStatic = *skinned;
    asStatic.calculateBounds();
    CHECK(asStatic.vertexCount() == 3);
    CHECK(asStatic.bounds.max.x == doctest::Approx(1.0f));

    // a consumer holding the base ref can discover + reach the skinning stream
    CHECK(asStatic.isSkinned());
    CHECK(asStatic.skinningStream().size() == 3);
    CHECK(asStatic.skinningStream()[0].joints[0] == 2);

    // and downcast safely (via the virtual isSkinned() tag, no RTTI)
    SkinnedMesh* down = toSkinned(&asStatic);
    REQUIRE(down != nullptr);
    CHECK(down->skeletonIndex == 3);
    CHECK(down->skinningDataSize() == 3 * 24);

    // a plain static mesh reports not-skinned + an empty stream
    StaticMesh plain;
    CHECK_FALSE(plain.isSkinned());
    CHECK(plain.skinningStream().size() == 0);
    CHECK(toSkinned(&plain) == nullptr);
}

TEST_CASE("primitives: cube + sphere + plane are well-formed")
{
    std::unique_ptr<StaticMesh> cube = Primitives::cube(2.0f);
    REQUIRE(cube);
    CHECK(cube->vertexCount() == 24);             // 4 verts x 6 faces (hard normals)
    CHECK(cube->indexCount() == 36);
    CHECK(cube->bounds.min.x == doctest::Approx(-1.0f));
    CHECK(cube->bounds.max.z == doctest::Approx(1.0f));

    std::unique_ptr<StaticMesh> sphere = Primitives::sphere(1.0f, 16, 8);
    REQUIRE(sphere);
    CHECK(sphere->indexCount() == 16 * 8 * 6);
    // every surface point is ~radius from the origin
    for (const StaticMeshVertex& v : sphere->vertices) {
        CHECK(length(v.position) == doctest::Approx(1.0f).epsilon(0.01));
    }

    std::unique_ptr<StaticMesh> plane = Primitives::plane(4.0f, 4.0f, 2, 2);
    REQUIRE(plane);
    CHECK(plane->vertexCount() == 9);             // (2+1) x (2+1)
    CHECK(plane->indexCount() == 2 * 2 * 6);
}

TEST_CASE("clear-for-reload empties in place (skinned clears both streams)")
{
    std::unique_ptr<SkinnedMesh> mesh = std::make_unique<SkinnedMesh>();
    mesh->vertices.push_back(StaticMeshVertex{});
    mesh->skinning.push_back(VertexSkinning{});
    mesh->skeletonIndex = 5;

    mesh->clearForReload();
    CHECK(mesh->vertexCount() == 0);
    CHECK(mesh->skinningStream().size() == 0);
    CHECK(mesh->skeletonIndex == -1);
}
