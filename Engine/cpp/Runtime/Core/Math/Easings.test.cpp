#include <doctest_with_main.h>

import core.stdtypes;
import core.math;

using namespace draco;
using namespace draco::math;

TEST_CASE("easings: endpoints map 0->0 and 1->1")
{
    const EasingFunction fns[] = {
        easeInQuadratic, easeOutQuadratic, easeInOutQuadratic,
        easeInCubic, easeOutCubic, easeInOutCubic,
        easeInQuartic, easeOutQuartic, easeInOutQuartic,
        easeInQuintic, easeOutQuintic, easeInOutQuintic,
        easeInSin, easeOutSin, easeInOutSin,
        easeInExponential, easeOutExponential, easeInOutExponential,
        easeInCircular, easeOutCircular, easeInOutCircular,
        easeInBack, easeOutBack, easeInOutBack,
        easeInElastic, easeOutElastic, easeInOutElastic,
        easeInBounce, easeOutBounce, easeInOutBounce,
    };
    for (EasingFunction f : fns) {
        CHECK(f(0.0f) == doctest::Approx(0.0f).epsilon(0.001));
        CHECK(f(1.0f) == doctest::Approx(1.0f).epsilon(0.001));
    }
}

TEST_CASE("easings: linear is the identity; quadratic is t*t")
{
    CHECK(easeInLinear(0.25f) == doctest::Approx(0.25f));
    CHECK(easeInLinear(0.9f) == doctest::Approx(0.9f));
    CHECK(easeInQuadratic(0.5f) == doctest::Approx(0.25f));
    CHECK(easeInCubic(0.5f) == doctest::Approx(0.125f));
}

TEST_CASE("easings: in/out symmetry at the midpoint of an ease-in-out")
{
    // ease-in-out families cross 0.5 at t=0.5.
    CHECK(easeInOutQuadratic(0.5f) == doctest::Approx(0.5f));
    CHECK(easeInOutCubic(0.5f) == doctest::Approx(0.5f));
    CHECK(easeInOutSin(0.5f) == doctest::Approx(0.5f));
}
