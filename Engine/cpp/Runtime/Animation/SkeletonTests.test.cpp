// Skeleton hierarchy: name lookup, root/child/order building, world-pose accumulation, and
// skinning-matrix correctness (identity at bind pose). No prior test existed; this covers the
// ported math directly.
#include <doctest_with_main.h>
#include <vector>
#include <span>
#include <string>

import core;
import animation;

using namespace draco;
using namespace draco::animation;

// A 3-bone chain root(0) -> child(1) -> grandchild(2), each translated +Y from its parent.
static void buildChain(Skeleton& s)
{
    std::vector<Bone>& bones = s.bones();
    bones[0].index = 0; bones[0].parentIndex = -1; bones[0].name = std::u8string{ u8"root" };
    bones[0].localBindPose.position = math::Vector3{ 0, 10, 0 };
    bones[1].index = 1; bones[1].parentIndex = 0;  bones[1].name = std::u8string{ u8"child" };
    bones[1].localBindPose.position = math::Vector3{ 0, 5, 0 };
    bones[2].index = 2; bones[2].parentIndex = 1;  bones[2].name = std::u8string{ u8"grandchild" };
    bones[2].localBindPose.position = math::Vector3{ 0, 2, 0 };
    s.buildNameMap();
    s.findRootBones();
    s.buildChildIndices();
}

TEST_CASE("skeleton: name map + root finding")
{
    Skeleton s{ 3 };
    buildChain(s);
    CHECK(s.boneCount() == 3);
    CHECK(s.findBone(u8"child") == 1);
    CHECK(s.findBone(u8"grandchild") == 2);
    CHECK(s.findBone(u8"missing") == -1);
    CHECK(s.rootBones().size() == 1);
    CHECK(s.rootBones()[0] == 0);
}

TEST_CASE("skeleton: world poses accumulate down the hierarchy (bind pose)")
{
    Skeleton s{ 3 };
    buildChain(s);

    math::Matrix4 world[3];
    s.computeWorldPoses(std::span<const BoneTransform>{}, std::span<math::Matrix4>{ world, 3 });

    // Pure translations compose additively along the chain (+Y).
    CHECK(math::nearlyEqual(world[0].m[3][1], 10.0f));
    CHECK(math::nearlyEqual(world[1].m[3][1], 15.0f));
    CHECK(math::nearlyEqual(world[2].m[3][1], 17.0f));
}

TEST_CASE("skeleton: skinning matrices are identity at the bind pose")
{
    Skeleton s{ 3 };
    buildChain(s);
    s.computeInverseBindPoses();

    math::Matrix4 skin[3];
    s.computeSkinningMatrices(std::span<const BoneTransform>{}, std::span<math::Matrix4>{ skin, 3 });

    // skin = inverseBind * world; evaluated at the bind pose this is identity for every bone.
    for (int i = 0; i < 3; ++i)
    {
        CHECK(math::nearlyEqual(skin[i].m[0][0], 1.0f));
        CHECK(math::nearlyEqual(skin[i].m[1][1], 1.0f));
        CHECK(math::nearlyEqual(skin[i].m[2][2], 1.0f));
        CHECK(math::nearlyEqual(skin[i].m[3][0], 0.0f));
        CHECK(math::nearlyEqual(skin[i].m[3][1], 0.0f));
        CHECK(math::nearlyEqual(skin[i].m[3][2], 0.0f));
    }
}

TEST_CASE("skeleton: hierarchical order lists parents before children")
{
    // A child declared BEFORE its parent in the array must still evaluate after it.
    Skeleton s{ 2 };
    std::vector<Bone>& bones = s.bones();
    bones[0].index = 0; bones[0].parentIndex = 1;   // bone 0's parent is bone 1
    bones[1].index = 1; bones[1].parentIndex = -1;  // bone 1 is the root
    bones[0].localBindPose.position = math::Vector3{ 0, 1, 0 };
    bones[1].localBindPose.position = math::Vector3{ 0, 100, 0 };
    s.findRootBones();
    s.buildChildIndices();

    math::Matrix4 world[2];
    s.computeWorldPoses(std::span<const BoneTransform>{}, std::span<math::Matrix4>{ world, 2 });
    // If order were wrong, bone 0 would miss bone 1's transform. Expect 100 + 1 = 101.
    CHECK(math::nearlyEqual(world[0].m[3][1], 101.0f));
    CHECK(math::nearlyEqual(world[1].m[3][1], 100.0f));
}
