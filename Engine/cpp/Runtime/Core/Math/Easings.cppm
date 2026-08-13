module;

#include <cmath>

export module core.math.easings;

import core.defs;
import core.math.constants;

// Standard easing functions: each maps an interpolation factor t in [0,1] to an eased
// value (also ~[0,1]). Pure functions over the scalar math; consumed by animation
// (EasingType) and UI transitions.
export namespace draco::math {

// A function mapping t in [0,1] to an eased interpolation factor.
using EasingFunction = f32 (*)(f32);

[[nodiscard]] inline f32 easeInLinear(f32 t) noexcept { return t; }
[[nodiscard]] inline f32 easeOutLinear(f32 t) noexcept { return t; }

// --- quadratic ---
[[nodiscard]] inline f32 easeInQuadratic(f32 t) noexcept { return t * t; }
[[nodiscard]] inline f32 easeOutQuadratic(f32 t) noexcept { return -1.0f * t * (t - 2.0f); }
[[nodiscard]] inline f32 easeInOutQuadratic(f32 t) noexcept
{
    t *= 2.0f;
    if (t < 1.0f) { return 0.5f * t * t; }
    t -= 1.0f;
    return -0.5f * (t * (t - 2.0f) - 1.0f);
}

// --- cubic ---
[[nodiscard]] inline f32 easeInCubic(f32 t) noexcept { return t * t * t; }
[[nodiscard]] inline f32 easeOutCubic(f32 t) noexcept { t -= 1.0f; return t * t * t + 1.0f; }
[[nodiscard]] inline f32 easeInOutCubic(f32 t) noexcept
{
    t *= 2.0f;
    if (t < 1.0f) { return 0.5f * t * t * t; }
    t -= 2.0f;
    return 0.5f * (t * t * t + 2.0f);
}

// --- quartic ---
[[nodiscard]] inline f32 easeInQuartic(f32 t) noexcept { return t * t * t * t; }
[[nodiscard]] inline f32 easeOutQuartic(f32 t) noexcept { t -= 1.0f; return -1.0f * (t * t * t * t - 1.0f); }
[[nodiscard]] inline f32 easeInOutQuartic(f32 t) noexcept
{
    t *= 2.0f;
    if (t < 1.0f) { return 0.5f * t * t * t * t; }
    t -= 2.0f;
    return -0.5f * (t * t * t * t - 2.0f);
}

// --- quintic ---
[[nodiscard]] inline f32 easeInQuintic(f32 t) noexcept { return t * t * t * t * t; }
[[nodiscard]] inline f32 easeOutQuintic(f32 t) noexcept { t -= 1.0f; return t * t * t * t * t + 1.0f; }
[[nodiscard]] inline f32 easeInOutQuintic(f32 t) noexcept
{
    t *= 2.0f;
    if (t < 1.0f) { return 0.5f * t * t * t * t * t; }
    t -= 2.0f;
    return 0.5f * (t * t * t * t * t + 2.0f);
}

// --- sinusoidal ---
[[nodiscard]] inline f32 easeInSin(f32 t) noexcept { return -1.0f * std::cos(t * PI2) + 1.0f; }
[[nodiscard]] inline f32 easeOutSin(f32 t) noexcept { return std::sin(t * PI2); }
[[nodiscard]] inline f32 easeInOutSin(f32 t) noexcept { return -0.5f * (std::cos(PI * t) - 1.0f); }

// --- exponential ---
[[nodiscard]] inline f32 easeInExponential(f32 t) noexcept
{
    if (t == 0.0f) { return 0.0f; }
    return std::pow(2.0f, 10.0f * (t - 1.0f));
}
[[nodiscard]] inline f32 easeOutExponential(f32 t) noexcept
{
    if (t == 1.0f) { return 1.0f; }
    return -std::pow(2.0f, -10.0f * t) + 1.0f;
}
[[nodiscard]] inline f32 easeInOutExponential(f32 t) noexcept
{
    if (t == 0.0f) { return 0.0f; }
    if (t == 1.0f) { return 1.0f; }
    t *= 2.0f;
    if (t < 1.0f) { return 0.5f * std::pow(2.0f, 10.0f * (t - 1.0f)); }
    t -= 1.0f;
    return 0.5f * (-std::pow(2.0f, -10.0f * t) + 2.0f);
}

// --- circular ---
[[nodiscard]] inline f32 easeInCircular(f32 t) noexcept { return -1.0f * (std::sqrt(1.0f - t * t) - 1.0f); }
[[nodiscard]] inline f32 easeOutCircular(f32 t) noexcept { t -= 1.0f; return std::sqrt(1.0f - t * t); }
[[nodiscard]] inline f32 easeInOutCircular(f32 t) noexcept
{
    t *= 2.0f;
    if (t < 1.0f) { return -0.5f * (std::sqrt(1.0f - t * t) - 1.0f); }
    t -= 2.0f;
    return 0.5f * (std::sqrt(1.0f - t * t) + 1.0f);
}

// --- back (overshoot) ---
[[nodiscard]] inline f32 easeInBack(f32 t) noexcept
{
    constexpr f32 s = 1.70158f;
    return t * t * ((s + 1.0f) * t - s);
}
[[nodiscard]] inline f32 easeOutBack(f32 t) noexcept
{
    constexpr f32 s = 1.70158f;
    t -= 1.0f;
    return t * t * ((s + 1.0f) * t + s) + 1.0f;
}
[[nodiscard]] inline f32 easeInOutBack(f32 t) noexcept
{
    constexpr f32 s  = 1.70158f;
    constexpr f32 s2 = s * 1.525f;
    t *= 2.0f;
    if (t < 1.0f) { return 0.5f * (t * t * ((s2 + 1.0f) * t - s2)); }
    t -= 2.0f;
    return 0.5f * (t * t * ((s2 + 1.0f) * t + s2) + 2.0f);
}

// --- elastic ---
[[nodiscard]] inline f32 easeInElastic(f32 t) noexcept
{
    if (t == 0.0f) { return 0.0f; }
    if (t == 1.0f) { return 1.0f; }
    constexpr f32 p = 0.3f;
    constexpr f32 s = p / 4.0f;
    t -= 1.0f;
    return -(std::pow(2.0f, 10.0f * t) * std::sin((t - s) * (2.0f * PI) / p));
}
[[nodiscard]] inline f32 easeOutElastic(f32 t) noexcept
{
    if (t == 0.0f) { return 0.0f; }
    if (t == 1.0f) { return 1.0f; }
    constexpr f32 p = 0.3f;
    constexpr f32 s = p / 4.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t - s) * (2.0f * PI) / p) + 1.0f;
}
[[nodiscard]] inline f32 easeInOutElastic(f32 t) noexcept
{
    if (t == 0.0f) { return 0.0f; }
    if (t == 1.0f) { return 1.0f; }
    t *= 2.0f;
    constexpr f32 p = 0.3f * 1.5f;
    constexpr f32 s = p / 4.0f;
    if (t < 1.0f)
    {
        t -= 1.0f;
        return -0.5f * (std::pow(2.0f, 10.0f * t) * std::sin((t - s) * (2.0f * PI) / p));
    }
    t -= 1.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t - s) * (2.0f * PI) / p) * 0.5f + 1.0f;
}

// --- bounce (out defined first; in/inout reference it) ---
[[nodiscard]] inline f32 easeOutBounce(f32 t) noexcept
{
    if (t < 1.0f / 2.75f) { return 7.5625f * t * t; }
    if (t < 2.0f / 2.75f) { t -= 1.5f / 2.75f;  return 7.5625f * t * t + 0.75f; }
    if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
    t -= 2.625f / 2.75f;
    return 7.5625f * t * t + 0.984375f;
}
[[nodiscard]] inline f32 easeInBounce(f32 t) noexcept { return 1.0f - easeOutBounce(1.0f - t); }
[[nodiscard]] inline f32 easeInOutBounce(f32 t) noexcept
{
    if (t < 0.5f) { return easeInBounce(t * 2.0f) * 0.5f; }
    return easeOutBounce(t * 2.0f - 1.0f) * 0.5f + 0.5f;
}

} // namespace draco::math
