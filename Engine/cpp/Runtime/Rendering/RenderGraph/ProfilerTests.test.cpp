// GPU-free guard/bounds coverage for GraphProfiler (Sedulous ships no profiler
// test; the timing path needs a real device + fence wait, exercised in samples).
#include <doctest_with_main.h>


import core;
import rendergraph;

using namespace draco;
using namespace draco::rendergraph;

TEST_CASE("rg.profiler: uninitialized is safe and reports zero")
{
    GraphProfiler profiler;
    CHECK(profiler.enabled);

    // No init() called: queries are zero, out-of-range indices are clamped.
    CHECK(profiler.getPassTimeMs(0) == 0.0f);
    CHECK(profiler.getPassTimeMs(-1) == 0.0f);
    CHECK(profiler.getPassTimeMs(1000) == 0.0f);

    profiler.setTimestampPeriod(1.0f);   // no-op without GPU state
    profiler.destroy();                  // safe with no device
    CHECK(profiler.getPassTimeMs(0) == 0.0f);
}
