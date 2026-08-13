// AnimationPlayer: playback, event firing, looping, evaluation. Ports the player section of
// the reference event suite + adds playback/eval coverage.
#include <doctest_with_main.h>
#include <vector>
#include <span>
#include <string_view>

import core;
import animation;

using namespace draco;
using namespace draco::animation;

// A 1-bone skeleton with a position clip moving +Y over 1s.
static void setupBone(Skeleton& s)
{
    s.bones()[0].index = 0; s.bones()[0].parentIndex = -1;
    s.findRootBones(); s.buildChildIndices(); s.computeInverseBindPoses();
}

TEST_CASE("player: event fires when time crosses (single update)")
{
    Skeleton skel{ 2 };
    setupBone(skel);
    AnimationPlayer player{ skel };
    int fireCount = 0;
    player.setEventHandler(AnimationEventHandler{ [&](std::u8string_view, f32) { ++fireCount; } });

    AnimationClip clip{ u8"Test", 1.0f };
    clip.addEvent(0.5f, u8"Hit");
    player.play(&clip);
    player.update(0.6f);
    CHECK(fireCount == 1);
}

TEST_CASE("player: events fire in order across updates")
{
    Skeleton skel{ 2 };
    setupBone(skel);
    AnimationPlayer player{ skel };
    std::vector<std::u8string_view> fired;
    player.setEventHandler(AnimationEventHandler{ [&](std::u8string_view n, f32) { fired.push_back(n); } });

    AnimationClip clip{ u8"Test", 2.0f };
    clip.addEvent(0.3f, u8"A"); clip.addEvent(0.8f, u8"B"); clip.addEvent(1.5f, u8"C");
    clip.sortEvents();
    player.play(&clip);
    player.update(0.5f); CHECK(fired.size() == 1); CHECK(fired[0] == std::u8string_view{ u8"A" });
    player.update(0.5f); CHECK(fired.size() == 2); CHECK(fired[1] == std::u8string_view{ u8"B" });
    player.update(0.5f); CHECK(fired.size() == 3); CHECK(fired[2] == std::u8string_view{ u8"C" });
}

TEST_CASE("player: looping clip fires event each loop + wraps time")
{
    Skeleton skel{ 2 };
    setupBone(skel);
    AnimationPlayer player{ skel };
    int fireCount = 0;
    player.setEventHandler(AnimationEventHandler{ [&](std::u8string_view, f32) { ++fireCount; } });

    AnimationClip clip{ u8"Test", 1.0f, /*looping*/ true };
    clip.addEvent(0.5f, u8"Hit");
    player.play(&clip);
    player.update(0.8f); CHECK(fireCount == 1);
    player.update(0.8f); CHECK(fireCount == 2);
    CHECK(player.currentTime() < 1.0f);                 // wrapped
    CHECK(player.state() == PlaybackState::Playing);    // looping never stops
}

TEST_CASE("player: non-looping clip clamps + stops at the end")
{
    Skeleton skel{ 2 };
    setupBone(skel);
    AnimationPlayer player{ skel };
    AnimationClip clip{ u8"Test", 1.0f, /*looping*/ false };
    player.play(&clip);
    player.update(2.0f);
    CHECK(math::nearlyEqual(player.currentTime(), 1.0f));
    CHECK(player.state() == PlaybackState::Stopped);
}

TEST_CASE("player: SetEventHandler replaces the old handler")
{
    Skeleton skel{ 2 };
    setupBone(skel);
    AnimationPlayer player{ skel };
    int count1 = 0, count2 = 0;
    AnimationClip clip{ u8"Test", 1.0f };
    clip.addEvent(0.5f, u8"Hit");

    player.setEventHandler(AnimationEventHandler{ [&](std::u8string_view, f32) { ++count1; } });
    player.play(&clip); player.update(0.6f);
    CHECK(count1 == 1); CHECK(count2 == 0);

    player.setEventHandler(AnimationEventHandler{ [&](std::u8string_view, f32) { ++count2; } });
    player.play(&clip); player.update(0.6f);
    CHECK(count1 == 1); CHECK(count2 == 1);
}

TEST_CASE("player: Evaluate drives skinning matrices from the clip")
{
    Skeleton skel{ 1 };
    skel.bones()[0].index = 0; skel.bones()[0].parentIndex = -1;
    skel.findRootBones(); skel.buildChildIndices(); skel.computeInverseBindPoses();

    AnimationClip clip{ u8"move", 1.0f };
    AnimationClip::Vec3Track* pos = clip.getOrCreatePositionTrack(0);
    pos->addKeyframe(0.0f, math::Vector3{ 0, 0, 0 });
    pos->addKeyframe(1.0f, math::Vector3{ 0, 10, 0 });

    AnimationPlayer player{ skel };
    player.play(&clip);
    player.setCurrentTime(1.0f);
    std::span<const math::Matrix4> skin = player.getSkinningMatrices();
    // At t=1 the bone translated +10 in Y from its (identity) bind pose -> skin matrix has Ty=10.
    CHECK(skin.size() == 1);
    CHECK(math::nearlyEqual(skin[0].m[3][1], 10.0f));
}
