// AnimationSampler: track/clip sampling + pose blending. Covers the sampling math directly.
#include <doctest_with_main.h>
#include <span>

import core;
import animation;

using namespace draco;
using namespace draco::animation;

TEST_CASE("sampler: math::Vector3 track linear interpolation")
{
    AnimationTrack<math::Vector3> track;
    track.addKeyframe(0.0f, math::Vector3{ 0, 0, 0 });
    track.addKeyframe(1.0f, math::Vector3{ 10, 0, 0 });
    CHECK(math::nearlyEqual(sampleVec3(&track, 0.0f), math::Vector3{ 0, 0, 0 }));
    CHECK(math::nearlyEqual(sampleVec3(&track, 0.5f), math::Vector3{ 5, 0, 0 }));
    CHECK(math::nearlyEqual(sampleVec3(&track, 1.0f), math::Vector3{ 10, 0, 0 }));
    CHECK(math::nearlyEqual(sampleVec3(&track, 2.0f), math::Vector3{ 10, 0, 0 }));   // clamps past last
    // empty / null returns the default
    CHECK(math::nearlyEqual(sampleVec3(nullptr, 0.5f, math::Vector3{ 9, 9, 9 }), math::Vector3{ 9, 9, 9 }));
}

TEST_CASE("sampler: Step interpolation holds previous value")
{
    AnimationTrack<math::Vector3> track;
    track.interpolation = InterpolationMode::Step;
    track.addKeyframe(0.0f, math::Vector3{ 0, 0, 0 });
    track.addKeyframe(1.0f, math::Vector3{ 10, 0, 0 });
    CHECK(math::nearlyEqual(sampleVec3(&track, 0.5f), math::Vector3{ 0, 0, 0 }));   // holds prev until next key
}

TEST_CASE("sampler: SampleClip animates targeted bones, bind pose otherwise")
{
    Skeleton skel{ 2 };
    skel.bones()[0].localBindPose.position = math::Vector3{ 0, 0, 0 };
    skel.bones()[1].localBindPose.position = math::Vector3{ 7, 7, 7 };

    AnimationClip clip{ u8"move", 1.0f };
    AnimationClip::Vec3Track* pos = clip.getOrCreatePositionTrack(0);
    pos->addKeyframe(0.0f, math::Vector3{ 0, 0, 0 });
    pos->addKeyframe(1.0f, math::Vector3{ 0, 10, 0 });

    BoneTransform poses[2] = {};
    sampleClip(clip, skel, 0.5f, std::span<BoneTransform>{ poses, 2 });
    CHECK(math::nearlyEqual(poses[0].position, math::Vector3{ 0, 5, 0 }));     // animated
    CHECK(math::nearlyEqual(poses[1].position, math::Vector3{ 7, 7, 7 }));     // untouched -> bind pose
}

TEST_CASE("sampler: BlendPoses + AdditivePoses")
{
    BoneTransform a[1] = {}; a[0].position = math::Vector3{ 0, 0, 0 };  a[0].scale = math::Vector3{ 1, 1, 1 };
    BoneTransform b[1] = {}; b[0].position = math::Vector3{ 10, 0, 0 }; b[0].scale = math::Vector3{ 3, 3, 3 };
    BoneTransform out[1] = {};

    blendPoses(std::span<const BoneTransform>{ a, 1 }, std::span<const BoneTransform>{ b, 1 }, 0.5f, std::span<BoneTransform>{ out, 1 });
    CHECK(math::nearlyEqual(out[0].position, math::Vector3{ 5, 0, 0 }));
    CHECK(math::nearlyEqual(out[0].scale, math::Vector3{ 2, 2, 2 }));

    BoneTransform base[1] = {}; base[0].position = math::Vector3{ 1, 0, 0 }; base[0].scale = math::Vector3{ 1, 1, 1 };
    BoneTransform add[1]  = {}; add[0].position  = math::Vector3{ 0, 5, 0 }; add[0].scale  = math::Vector3{ 1, 1, 1 };
    additivePoses(std::span<const BoneTransform>{ base, 1 }, std::span<const BoneTransform>{ add, 1 }, 1.0f, std::span<BoneTransform>{ out, 1 });
    CHECK(math::nearlyEqual(out[0].position, math::Vector3{ 1, 5, 0 }));   // base + additive*weight
}
