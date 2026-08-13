#include <doctest_with_main.h>
#include <cmath>

import core;
import animation;

using namespace draco;
using namespace draco::animation;

TEST_CASE("easing: Linear returns input unchanged")
{
    CHECK(applyEasing(EasingType::Linear, 0.0f) == 0.0f);
    CHECK(applyEasing(EasingType::Linear, 0.5f) == 0.5f);
    CHECK(applyEasing(EasingType::Linear, 1.0f) == 1.0f);
}

TEST_CASE("easing: ToFunction non-null + Apply boundary values for all types")
{
    for (i32 i = 0; i < static_cast<i32>(EasingType::Count); ++i)
    {
        const EasingType type = static_cast<EasingType>(i);
        CHECK(toFunction(type) != nullptr);
        CHECK(std::abs(applyEasing(type, 0.0f)) < 0.01f);
        CHECK(std::abs(applyEasing(type, 1.0f) - 1.0f) < 0.01f);
    }
}

TEST_CASE("easing: in/out shape at midpoint")
{
    CHECK(applyEasing(EasingType::EaseInQuadratic, 0.5f) < 0.5f);    // slow start
    CHECK(applyEasing(EasingType::EaseOutQuadratic, 0.5f) > 0.5f);   // fast start
    CHECK(std::abs(applyEasing(EasingType::EaseInOutCubic, 0.5f) - 0.5f) < 0.01f);   // symmetric
}
