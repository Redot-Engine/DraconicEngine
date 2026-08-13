// AnimationEvent + AnimationClip event storage + FireEvents. Ported from the applicable parts of
// the reference event suite (Player/ClipStateNode/BlendTree parts land later).
#include <doctest_with_main.h>
#include <vector>
#include <string_view>

import core;
import animation;

using namespace draco;
using namespace draco::animation;

TEST_CASE("event: constructor sets time and name")
{
    const AnimationEvent e{ 0.5f, u8"Footstep" };
    CHECK(e.time == 0.5f);
    CHECK(e.name == std::u8string_view{ u8"Footstep" });
}

TEST_CASE("clip: AddEvent increases count")
{
    AnimationClip clip{ u8"Test", 2.0f };
    CHECK(clip.events().size() == 0);
    clip.addEvent(0.5f, u8"Hit");
    CHECK(clip.events().size() == 1);
    clip.addEvent(1.0f, u8"Sound");
    CHECK(clip.events().size() == 2);
}

TEST_CASE("clip: SortEvents sorts by time")
{
    AnimationClip clip{ u8"Test", 2.0f };
    clip.addEvent(1.5f, u8"C");
    clip.addEvent(0.2f, u8"A");
    clip.addEvent(0.8f, u8"B");
    clip.sortEvents();
    CHECK(clip.events()[0].time == 0.2f);
    CHECK(clip.events()[1].time == 0.8f);
    CHECK(clip.events()[2].time == 1.5f);
    CHECK(clip.events()[0].name == std::u8string_view{ u8"A" });
    CHECK(clip.events()[2].name == std::u8string_view{ u8"C" });
}

TEST_CASE("clip: FireEvents crossing behavior")
{
    {   // crosses threshold -> fires
        AnimationClip clip{ u8"Test", 2.0f };
        clip.addEvent(0.5f, u8"Hit");
        int count = 0;
        clip.fireEvents(0.0f, 1.0f, [&](std::u8string_view, f32) { ++count; });
        CHECK(count == 1);
    }
    {   // before threshold -> no fire
        AnimationClip clip{ u8"Test", 2.0f };
        clip.addEvent(1.5f, u8"Hit");
        int count = 0;
        clip.fireEvents(0.0f, 1.0f, [&](std::u8string_view, f32) { ++count; });
        CHECK(count == 0);
    }
    {   // exact end time fires (inclusive right)
        AnimationClip clip{ u8"Test", 1.0f };
        clip.addEvent(0.5f, u8"Exact");
        int count = 0;
        clip.fireEvents(0.3f, 0.5f, [&](std::u8string_view, f32) { ++count; });
        CHECK(count == 1);
    }
}

TEST_CASE("clip: FireEvents in order + loop wrap + non-looping past duration")
{
    {   // multiple fire in order
        AnimationClip clip{ u8"Test", 2.0f };
        clip.addEvent(0.3f, u8"A"); clip.addEvent(0.7f, u8"B"); clip.addEvent(1.2f, u8"C");
        clip.sortEvents();
        std::vector<std::u8string_view> fired;
        clip.fireEvents(0.0f, 1.5f, [&](std::u8string_view n, f32) { fired.push_back(n); });
        CHECK(fired.size() == 3);
        CHECK(fired[0] == std::u8string_view{ u8"A" });
        CHECK(fired[1] == std::u8string_view{ u8"B" });
        CHECK(fired[2] == std::u8string_view{ u8"C" });
    }
    {   // looping wrap: prev 0.9 -> cur 1.3 on a 1.0s loop fires only the early (post-wrap) event
        AnimationClip clip{ u8"Test", 1.0f, true };
        clip.addEvent(0.2f, u8"Early"); clip.addEvent(0.8f, u8"Late");
        clip.sortEvents();
        std::vector<std::u8string_view> fired;
        clip.fireEvents(0.9f, 1.3f, [&](std::u8string_view n, f32) { fired.push_back(n); });
        CHECK(fired.size() == 1);
        CHECK(fired[0] == std::u8string_view{ u8"Early" });
    }
    {   // non-looping past duration fires remaining up to duration
        AnimationClip clip{ u8"Test", 1.0f, false };
        clip.addEvent(0.8f, u8"NearEnd"); clip.addEvent(1.0f, u8"AtEnd");
        clip.sortEvents();
        int count = 0;
        clip.fireEvents(0.5f, 1.5f, [&](std::u8string_view, f32) { ++count; });
        CHECK(count == 2);
    }
}

TEST_CASE("clip: ComputeDuration from latest keyframe")
{
    AnimationClip clip{ u8"Test" };
    AnimationClip::Vec3Track* pos = clip.getOrCreatePositionTrack(0);
    pos->addKeyframe(0.0f, math::Vector3{ 0, 0, 0 });
    pos->addKeyframe(1.25f, math::Vector3{ 1, 0, 0 });
    clip.computeDuration();
    CHECK(math::nearlyEqual(clip.duration, 1.25f));
    // GetOrCreate returns the SAME track for the same bone.
    CHECK(clip.getOrCreatePositionTrack(0) == pos);
    CHECK(clip.positionTracks().size() == 1);
}
