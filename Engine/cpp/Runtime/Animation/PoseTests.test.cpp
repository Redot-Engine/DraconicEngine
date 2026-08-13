#include <doctest_with_main.h>
#include <span>

import core;
import animation;

using namespace draco;
using namespace draco::animation;

TEST_CASE("pose: constructor with bone transforms sets bone count")
{
    BoneTransform transforms[4] = {};
    const AnimationPose pose{ std::span<BoneTransform>{ transforms, 4 } };
    CHECK(pose.boneCount() == 4);
    CHECK(pose.hasMorphWeights() == false);
}

TEST_CASE("pose: constructor with morph weights")
{
    BoneTransform transforms[2] = {};
    f32 morphs[3] = { 0.5f, 0.0f, 1.0f };
    const AnimationPose pose{ std::span<BoneTransform>{ transforms, 2 }, std::span<f32>{ morphs, 3 } };
    CHECK(pose.boneCount() == 2);
    CHECK(pose.hasMorphWeights() == true);
    CHECK(pose.morphWeights.size() == 3);
}

TEST_CASE("pose: empty pose has zero bones")
{
    const AnimationPose pose{ std::span<BoneTransform>{} };
    CHECK(pose.boneCount() == 0);
    CHECK(pose.hasMorphWeights() == false);
}

TEST_CASE("pose: bone transforms are accessible")
{
    BoneTransform transforms[2] = {};
    transforms[0].position = math::Vector3{ 1, 2, 3 };
    transforms[1].position = math::Vector3{ 4, 5, 6 };
    const AnimationPose pose{ std::span<BoneTransform>{ transforms, 2 } };
    CHECK(pose.boneTransforms[0].position.x == 1.0f);
    CHECK(pose.boneTransforms[1].position.x == 4.0f);
}
