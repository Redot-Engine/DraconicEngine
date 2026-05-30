module;

#include <cmath>
#include <concepts>
#include <limits>

export module core.math.functions;
import core.math.constants;
import core.defs;
import core.stdtypes;

export namespace draco::math {
    template <arithmetic T>
    constexpr T min(T a, T b) {
        return a < b ? a : b;
    }

    template <arithmetic T>
    constexpr T max(T a, T b) {
        return b < a ? a : b;
    }

    template <arithmetic T>
    constexpr T sqr(T x) noexcept { return x*x; }

    template <std::floating_point T>
    [[nodiscard]] constexpr bool isNan(T val) noexcept {
        // Only NaN does not equal itself.
        return val != val;
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr bool isInf(T val) noexcept {
        return std::isinf(val);
    }

    template <std::floating_point T>
    [[nodiscard]] constexpr bool isFinite(T val) noexcept {
        return std::isfinite(val);
    }

    template <arithmetic T>
    constexpr T abs(T value) noexcept {
        // Manually compute abs for signed types.
        // Also avoids potential i8 -> i32 issues.
        if constexpr (std::floating_point<T>) {
            return value < T{0} ? -value : value;
        } else if constexpr (std::signed_integral<T>) {
            if (value == std::numeric_limits<T>::min()) {
                return std::numeric_limits<T>::max(); // define saturating behavior explicitly
            }
            return value < T{0} ? -value : value;
        } else {
            // unsigned is always positive! :^)
            return value;
        }
    }

	template <arithmetic T>
	constexpr T sign(T value) noexcept {
        if constexpr (std::floating_point<T>) {
			if (value != value) {
				return value;
			} else if (value) {
            	return value < T{0} ? T{-1} : T{1};
			}
			return T{0};
        } else if constexpr (std::signed_integral<T>) {
            if (value) {
            	return value < T{0} ? T{-1} : T{1};
            }
			return T{0};
        } else {
            return T{value != T{0}};
        }
	}

	constexpr f32 floor(f32 value) noexcept {
		if (value != value || abs(value) >= DECIMAL_LIMIT_F) {
			return value;
		}
		const f32 truncated = static_cast<i32>(value);
		return truncated - (value < truncated);
	}

	constexpr f32 ceil(f32 value) noexcept {
		return -floor(-value);
	}

	constexpr f32 trunc(f32 value) noexcept {
		if (value != value || abs(value) >= DECIMAL_LIMIT_F) {
			return value;
		}
		return static_cast<i32>(value);
	}

	constexpr f32 round(f32 value) noexcept {
		const f32 s = sign(value);
		return s * floor(s * value + 0.5f);
	}

    template <std::floating_point T>
    constexpr T degToRad(T y) noexcept {
        return y * (T{PI} / T{180.});
    }

    template <std::floating_point T>
    constexpr T radToDeg(T y) noexcept {
        return y * (T{180.} / T{PI});
    }

    template <std::floating_point T>
    T pow(T x, T y) {
        return static_cast<T>(std::pow(x, y));
    }

    template <std::floating_point T>
    constexpr T lerp(T from, T to, T weight) noexcept {
        return std::lerp(from, to, weight);
    }

    template <std::floating_point T>
    constexpr T cubicInterpolate(T from, T to, T before, T after, T weight) noexcept {
        // weight squared.
        T w2 = weight * weight;
        // weight cubed.
        T w3 = weight * w2;

        // calculate coefficients.
        T a = -before + to;
        T b = T{2} * before - T{5} * from + T{4} * to - after;
        T c = -before + T{3} * from - T{3} * to + after;

        // Catmull-Rom Interpolation:
        // 0.5 * ((2 * p_from) + (a * w) + (b * w^2) + (c * w^3))

        if consteval {
            // compile time
            return T{0.5} * (T{2.}*from + a*weight + b*w2 + c*w3);
        } else {
            // runtime
            return T{0.5} * std::fma(c, w3, std::fma(b, w2, std::fma(a, weight, T{2} * from)));
        }
    }

    template <std::floating_point T>
    constexpr T cubicInterpolateInTime(
        T from, T to,
        T before, T after, T weight,
        T to_t, T before_t, T after_t) noexcept {
        /* Barry-Goldman method */
        T t = lerp(T{0.}, to_t, weight);

        // At least try to make this easier to parse for others.
        T pre_scale = before_t == T{0.} ? T{0.} : (t - before_t) / -before_t;
        T to_scale = (to_t == T{0.}) ? T{.5} : t / to_t;
        T post_range = after_t - to_t;
        T post_scale = (post_range == T{0.}) ? T{1.} : (t - to_t) / post_range;

        // First layer.
        T a1 = lerp(before, from, pre_scale);
        T a2 = lerp(from, to, to_scale);
        T a3 = lerp(to, after, post_scale);

        // More parsing.
        T mid_range = to_t - before_t;
        T from_to_scale = (mid_range == T{0.}) ? T{0.} : (t - before_t) / mid_range;
        T to_post_scale = (after_t == T{0.}) ? T{1.} : t / after_t;

        // Second layer.
        T b1 = lerp(a1, a2, from_to_scale);
        T b2 = lerp(a2, a3, to_post_scale);

        // One more for the road.
        T final_scale = (to_t == T{0.}) ? T{.5} : t / to_t;

        return lerp(b1, b2, final_scale);
    }

    template <std::floating_point T>
    constexpr T bezierInterpolate(T start, T control_1, T control_2, T end, T t) noexcept {
        /* Formula from Wikipedia article on Bezier curves. */
        // one minus t.
        T omt = T{1.} - t;
        T omt2 = omt * omt;
        T omt3 = omt2 * omt;
        T t2 = t * t;
        T t3 = t2 * t;

        // B(t) = (1-t)^3 * P_0 + 3(1 - t)^2 * t * P_1 + 3(1 - t) * t^2 * P_2 + t^3 * P_3
        T d = start * omt3 + control_1 * omt2 * t * T{3.} + control_2 * omt * t2 * T{3.} + end * t3;
        return d;
    }

    template <std::floating_point T>
    constexpr T bezierDerivative(T start, T control_1, T control_2, T end, T t) noexcept {
        /* Formula from Wikipedia article on Bezier curves. */
        T omt = T{1.} - t;
        T omt2 = omt * omt;
        T t2 = t * t;

        T d = (control_1 - start) * T{3.} * omt2 + (control_2 - control_1) * T{6.} * omt * t + (end - control_2) * T{3.} * t2;
        return d;
    }
}
