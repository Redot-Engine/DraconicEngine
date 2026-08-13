#include <doctest_with_main.h>
#include <string>
#include <string_view>

import core;
import profiler;

using namespace draco;
using namespace draco::profiler;

TEST_CASE("profiler: a single scope is recorded")
{
    Profiler& p = Profiler::get();
    p.setEnabled(true);
    p.beginFrame();
    { ScopedProfile s("Alpha"); }
    p.endFrame();

    const ProfileFrame& f = p.completedFrame();
    REQUIRE(f.samples.size() == 1u);
    CHECK(std::u8string_view(reinterpret_cast<const char8_t*>(f.samples[0].name)) == std::u8string_view(u8"Alpha"));
    CHECK(f.samples[0].depth == 0u);
}

TEST_CASE("profiler: nested scopes get increasing depth")
{
    Profiler& p = Profiler::get();
    p.setEnabled(true);
    p.beginFrame();
    {
        ScopedProfile outer("Outer");
        { ScopedProfile inner("Inner"); }
        { ScopedProfile inner2("Inner2"); }
    }
    p.endFrame();

    const ProfileFrame& f = p.completedFrame();
    REQUIRE(f.samples.size() == 3u);
    // endScope records on close, so children appear before the parent; find by name.
    u32 outerDepth = 99, innerDepth = 99;
    for (usize i = 0; i < f.samples.size(); ++i) {
        std::u8string_view n(reinterpret_cast<const char8_t*>(f.samples[i].name));
        if (n == std::u8string_view(u8"Outer")) { outerDepth = f.samples[i].depth; }
        if (n == std::u8string_view(u8"Inner")) { innerDepth = f.samples[i].depth; }
    }
    CHECK(outerDepth == 0u);
    CHECK(innerDepth == 1u);
}

TEST_CASE("profiler: disabled records nothing")
{
    Profiler& p = Profiler::get();
    p.setEnabled(true);
    p.beginFrame(); p.endFrame();                       // clean baseline: an empty completed frame
    REQUIRE(p.completedFrame().samples.size() == 0u);

    p.setEnabled(false);
    p.beginFrame();
    { ScopedProfile s("Ignored"); }
    p.endFrame();
    p.setEnabled(true);
    CHECK(p.completedFrame().samples.size() == 0u);     // unchanged - disabled did nothing
}

TEST_CASE("profiler: frame number advances and a report builds")
{
    Profiler& p = Profiler::get();
    p.setEnabled(true);
    p.beginFrame();
    { ScopedProfile s("Work"); }
    p.endFrame();
    const u64 n0 = p.completedFrame().frameNumber;

    p.beginFrame();
    { ScopedProfile s("Work"); }
    p.endFrame();
    CHECK(p.completedFrame().frameNumber == n0 + 1u);

    std::u8string report = p.buildReport();
    CHECK(report.size() > 0u);
}
