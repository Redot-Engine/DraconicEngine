module;

#include <cmath>
#include <format>
#include "Runtime/Platform/intrinsics.h"

export module core.math.types:vector4;
export import :common;

import core.math.constants;
import core.math.functions;
import core.defs;
import core.stdtypes;

export namespace draco::math {
    // assertions
    static_assert(sizeof(Vector4) == 16, "Vector4 must be 16 bytes");
    static_assert(alignof(Vector4) == 16, "Vector4 must be 16-byte aligned");
    static_assert(trivial<Vector4>, "Vector4 must be trivial");
    static_assert(std::is_standard_layout_v<Vector4>, "Vector4 must be standard layout");

    // constructors
    [[nodiscard]] constexpr Vector4::Vector4(const f32 n) noexcept
        : x{n}, y{n}, z{n}, w{n} { }

    [[nodiscard]] constexpr Vector4::Vector4(const f32 x, const f32 y, const f32 z, const f32 w) noexcept
        : x{x}, y{y}, z{z}, w{w} { }

    [[nodiscard]] constexpr Vector4::Vector4(const Vector2& xy) noexcept
        : x{xy.x}, y{xy.y}, z{0.0f}, w{0.0f} { }

    [[nodiscard]] constexpr Vector4::Vector4(const Vector2& xy, const f32 z, const f32 w) noexcept
        : x{xy.x}, y{xy.y}, z{z}, w{w} { }

    [[nodiscard]] constexpr Vector4::Vector4(const f32 x, const Vector2& yz, const f32 w) noexcept
        : x{x}, y{yz.x}, z{yz.y}, w{w} { }

    [[nodiscard]] constexpr Vector4::Vector4(const f32 x, const f32 y, const Vector2& zw) noexcept
        : x{x}, y{y}, z{zw.x}, w{zw.y} { }

    [[nodiscard]] constexpr Vector4::Vector4(const Vector2& xy, const Vector2& zw) noexcept
        : x{xy.x}, y{xy.y}, z{zw.x}, w{zw.y} { }

    [[nodiscard]] constexpr Vector4::Vector4(const Vector3& xyz, const f32 w) noexcept
        : x{xyz.x}, y{xyz.y}, z{xyz.z}, w{w} { }

    [[nodiscard]] constexpr Vector4::Vector4(const f32 x, const Vector3& yzw) noexcept
        : x{x}, y{yzw.x}, z{yzw.y}, w{yzw.z} { }
    
    // static
    [[nodiscard]] constexpr Vector4 Vector4::xAxis(const f32 x) noexcept {
        return { x, 0.0f, 0.0f, 0.0f };
    }

    [[nodiscard]] constexpr Vector4 Vector4::yAxis(const f32 y) noexcept {
        return { 0.0f, y, 0.0f, 0.0f };
    }

    [[nodiscard]] constexpr Vector4 Vector4::zAxis(const f32 z) noexcept {
        return { 0.0f, 0.0f, z, 0.0f };
    }

    [[nodiscard]] constexpr Vector4 Vector4::wAxis(const f32 w) noexcept {
        return { 0.0f, 0.0f, 0.0f, w };
    }

    // element access
    [[nodiscard]] constexpr f32& Vector4::operator[](const i32 i) noexcept {
        if consteval {
            return select(i, x, y, z, w);
        } else { return (&x)[i]; }
    }

    [[nodiscard]] constexpr const f32& Vector4::operator[](const i32 i) const noexcept {
        if consteval {
            return select(i, x, y, z, w);
        } else { return (&x)[i]; }
    }

    // swizzle
    [[nodiscard]] constexpr Vector2 Vector4::operator[](const i32 i0, const i32 i1) const noexcept {
        if consteval {
            return { select(i0, x, y, z, w), select(i1, x, y, z, w) };
        } else {
            return { (&x)[i0], (&x)[i1] };
        }
    }

    [[nodiscard]] constexpr Vector3 Vector4::operator[](const i32 i0, const i32 i1, const i32 i2) const noexcept {
        if consteval {
            return { select(i0, x, y, z, w), select(i1, x, y, z, w), select(i2, x, y, z, w) };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2] };
        }
    }

    [[nodiscard]] constexpr Vector4 Vector4::operator[](const i32 i0, const i32 i1, const i32 i2, const i32 i3) const noexcept {
        if consteval {
            return { select(i0, x, y, z, w), select(i1, x, y, z, w), select(i2, x, y, z, w), select(i3, x, y, z, w)  };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2], (&x)[i3] };
        }
    }

    // operators
    constexpr Vector4& Vector4::operator+=(const Vector4& other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        w += other.w;
        return *this;
    }

    constexpr Vector4& Vector4::operator+=(const f32 other) noexcept {
        x += other;
        y += other;
        z += other;
        w += other;
        return *this;
    }

    constexpr Vector4& Vector4::operator-=(const Vector4& other) noexcept {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }

    constexpr Vector4& Vector4::operator-=(const f32 other) noexcept {
        x -= other;
        y -= other;
        z -= other;
        w -= other;
        return *this;
    }

    constexpr Vector4& Vector4::operator*=(const Vector4& other) noexcept {
        x *= other.x;
        y *= other.y;
        z *= other.z;
        w *= other.w;
        return *this;
    }

    constexpr Vector4& Vector4::operator*=(const f32 other) noexcept {
        x *= other;
        y *= other;
        z *= other;
        w *= other;
        return *this;
    }

    constexpr Vector4& Vector4::operator/=(const Vector4& other) noexcept {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        w /= other.w;
        return *this;
    }

    constexpr Vector4& Vector4::operator/=(const f32 other) noexcept {
        const f32 inv = 1.0f / other;
        x *= inv;
        y *= inv;
        z *= inv;
        w *= inv;
        return *this;
    }

    constexpr Vector4& Vector4::operator=(const f32 other) noexcept {
        x = other;
        y = other;
        z = other;
        w = other;
        return *this;
    }

    [[nodiscard]] constexpr Vector4 Vector4::operator+() const noexcept {
        return { x, y, z, w };
    }

    [[nodiscard]] constexpr Vector4 Vector4::operator-() const noexcept {
        return { -x, -y, -z, -w };
    }
    
    [[nodiscard]] constexpr Vector4 operator+(const Vector4& a, const Vector4& b) noexcept {
        return { a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w };
    }

    [[nodiscard]] constexpr Vector4 operator+(const Vector4& a, const f32 b) noexcept {
        return { a.x+b, a.y+b, a.z+b, a.w+b };
    }

    [[nodiscard]] constexpr Vector4 operator+(const f32 a, const Vector4& b) noexcept {
        return b+a;
    }

    [[nodiscard]] constexpr Vector4 operator-(const Vector4& a, const Vector4& b) noexcept {
        return { a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w };
    }

    [[nodiscard]] constexpr Vector4 operator-(const Vector4& a, const f32 b) noexcept {
        return { a.x-b, a.y-b, a.z-b, a.w-b };
    }

    [[nodiscard]] constexpr Vector4 operator-(const f32 a, const Vector4& b) noexcept {
        return { a-b.x, a-b.y, a-b.z, a-b.w };
    }

    [[nodiscard]] constexpr Vector4 operator*(const Vector4& a, const Vector4& b) noexcept {
        return { a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w };
    }

    [[nodiscard]] constexpr Vector4 operator*(const Vector4& a, const f32 b) noexcept {
        return { a.x*b, a.y*b, a.z*b, a.w*b };
    }

    [[nodiscard]] constexpr Vector4 operator*(const f32 a, const Vector4& b) noexcept {
        return b*a;
    }

    [[nodiscard]] constexpr Vector4 operator/(const Vector4& a, const Vector4& b) noexcept {
        return { a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w };
    }
    
    [[nodiscard]] constexpr Vector4 operator/(const Vector4& a, const f32 b) noexcept {
        return a * (1.0f / b);
    }
    
    [[nodiscard]] constexpr Vector4 operator/(const f32 a, const Vector4& b) noexcept {
        return { a/b.x, a/b.y, a/b.z, a/b.w };
    }

    // functions
    
    // Returns dot product
    [[nodiscard]] constexpr f32 dot(const Vector4& a, const Vector4& b) noexcept {
        if !consteval {
            #if ARCH_X64
                // There's only 4 floats, so SSE is what we will use.
                // If there's a situation with multiple dot calls, we can setup a
                // way to call 8 / 16 / 32 floats, but over-head could upset gains.
                // Be sure it occurs commonly enough to matter here.
                // Shuffle-first reduction worked best here.
                __m128 va = _mm_load_ps(&a.x);
                __m128 vb = _mm_load_ps(&b.x);

                __m128 m = _mm_mul_ps(va, vb);

                __m128 shuf = _mm_movehdup_ps(m);
                __m128 sum  = _mm_add_ps(m, shuf);

                shuf = _mm_movehl_ps(shuf, sum);
                sum = _mm_add_ss(sum, shuf);

                return _mm_cvtss_f32(sum);
            #elif ARCH_ARM64
                #error "ARM64 NEON support not yet implemented."
            #endif
        }

        return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    }

    // Returns squared magnitude
    [[nodiscard]] constexpr f32 length_sq(const Vector4& v) noexcept {
        return dot(v, v);
    }

    // Returns magnitude
    [[nodiscard]] f32 length(const Vector4& v) noexcept {
        return std::sqrt(length_sq(v));
    }

    // Return squared distance between two vectors
    [[nodiscard]] constexpr f32 distance_sq(const Vector4& a, const Vector4& b) noexcept {
        return length_sq(a - b);
    }

    // Returns distance between two vectors
    [[nodiscard]] f32 distance(const Vector4& a, const Vector4& b) noexcept {
        return length(a - b);
    }

    // Safe normalize, checks length
    [[nodiscard]] Vector4 normalize(const Vector4& v) noexcept {
        const f32 len = length(v);

        return (len > CMP_NORMALIZE_TOLERANCE) ? v / len : Vector4();
    }
    
    // Faster normalize, it presupposes vector has non-zero length
    // TODO: add check that v is non-zero on debug builds
    [[nodiscard]] Vector4 normalize_fast(const Vector4& v) noexcept {
        return v / length(v);
    }

    // Returns vector projected onto normal
    [[nodiscard]] constexpr Vector4 project(const Vector4& vector, const Vector4& normal) noexcept {
        return normal * (dot(vector, normal) / length_sq(normal));
    }

    // Returns a vector reflected off a plane defined by its normal
    [[nodiscard]] constexpr Vector4 reflect(const Vector4& incoming, const Vector4& normal) noexcept {
        return incoming - 2.0f * dot(incoming, normal) * normal;
    }

    // Returns the angle between two vectors
    [[nodiscard]] f32 angle(const Vector4& a, const Vector4& b) noexcept {
        return std::acos(dot(a, b) / (length(a) * length(b)));
    }

    // Returns linear interpolation between two vectors
    [[nodiscard]] constexpr Vector4 lerp(const Vector4& from, const Vector4& to, const f32 weight) noexcept {
        return {
            lerp(from.x, to.x, weight),
            lerp(from.y, to.y, weight),
            lerp(from.z, to.z, weight),
            lerp(from.w, to.w, weight)
        };
    }

    // Returns component-wise minimum
    [[nodiscard]] constexpr Vector4 min(const Vector4& a, const Vector4& b) noexcept {
        return {
            std::min(a.x, b.x),
            std::min(a.y, b.y),
            std::min(a.z, b.z),
            std::min(a.w, b.w)
        };
    }

    [[nodiscard]] constexpr Vector4 min(const Vector4& a, const f32 b) noexcept {
        return {
            std::min(a.x, b),
            std::min(a.y, b),
            std::min(a.z, b),
            std::min(a.w, b)
        };
    }

    [[nodiscard]] constexpr Vector4 min(const f32 a, const Vector4& b) noexcept {
        return min(b, a);
    }

    // Returns the vector with the smaller length
    [[nodiscard]] constexpr Vector4 min_length(const Vector4& a, const Vector4& b) noexcept {
        return length_sq(a) < length_sq(b) ? a : b;
    }

    // Returns a vector in the same direction whose length is bounded above by the given value.
    [[nodiscard]] Vector4 min_length(const Vector4& a, const f32 b) noexcept {
        const f32 len_sq = length_sq(a);
        
        if (len_sq > b * b) {
            return a * (b / std::sqrt(len_sq));
        } else {
            return a;
        }
    }

    [[nodiscard]] Vector4 min_length(const f32 a, const Vector4& b) noexcept {
        return min_length(b, a);
    }

    // Returns component-wise maximum
    [[nodiscard]] constexpr Vector4 max(const Vector4& a, const Vector4& b) noexcept {
        return {
            std::max(a.x, b.x),
            std::max(a.y, b.y),
            std::max(a.z, b.z),
            std::max(a.w, b.w)
        };
    }

    [[nodiscard]] constexpr Vector4 max(const Vector4& a, const f32 b) noexcept {
        return {
            std::max(a.x, b),
            std::max(a.y, b),
            std::max(a.z, b),
            std::max(a.w, b)
        };
    }

    [[nodiscard]] constexpr Vector4 max(const f32 a, const Vector4& b) noexcept {
        return max(b, a);
    }

    // Returns the vector with the larger length
    [[nodiscard]] constexpr Vector4 max_length(const Vector4& a, const Vector4& b) noexcept {
        return length_sq(a) > length_sq(b) ? a : b;
    }

    // Returns a vector in the same direction whose length is bounded below by the given value. Returns the 0 vector if the vector is too small to be normalized.
    [[nodiscard]] Vector4 max_length(const Vector4& a, const f32 b) noexcept {
        const f32 len_sq = length_sq(a);
        
        if (len_sq <= CMP_NORMALIZE_TOLERANCE2) {
            return Vector4();
        } else if (len_sq < b * b) {
            return a * (b / std::sqrt(len_sq));
        } else {
            return a;
        }
    }

    [[nodiscard]] Vector4 max_length(const f32 a, const Vector4& b) noexcept {
        return max_length(b, a);
    }

    // Clamps each component of x to the range [x_min, x_max]. Presupposes x_min <= x_max.
    [[nodiscard]] constexpr Vector4 clamp(const Vector4& x, const Vector4& x_min, const Vector4& x_max) noexcept {
        return max(x_min, min(x, x_max));
    }

    [[nodiscard]] constexpr Vector4 clamp(const Vector4& x, const f32 x_min, const f32 x_max) noexcept {
        return max(x_min, min(x, x_max));
    }

    // Clamps the length of the vector to the range [x_min, x_max]. Presupposes x_min <= x_max. Returns the 0 vector if the vector is too small to be normalized.
    [[nodiscard]] Vector4 clamp_length(const Vector4& v, const f32 x_min, const f32 x_max) noexcept {
        const f32 len_sq = length_sq(v);
        
        if (len_sq <= CMP_NORMALIZE_TOLERANCE2) {
            return Vector4();
        } else if (len_sq < x_min * x_min) {
            return v * (x_min / std::sqrt(len_sq));
        } else if (len_sq > x_max * x_max) {
            return v * (x_max / std::sqrt(len_sq));
        } else {
            return v;
        }
    }

    // Returns component-wise absolute value
    [[nodiscard]] constexpr Vector4 abs(const Vector4& v) noexcept {
        return {
            abs(v.x),
            abs(v.y),
            abs(v.z),
            abs(v.w)
        };
    }

    // Returns component-wise floor
    [[nodiscard]] constexpr Vector4 floor(const Vector4& v) noexcept {
        return {
            floor(v.x),
            floor(v.y),
            floor(v.z),
            floor(v.w)
        };
    }

    // Returns component-wise ceiling
    [[nodiscard]] constexpr Vector4 ceil(const Vector4& v) noexcept {
        return {
            ceil(v.x),
            ceil(v.y),
            ceil(v.z),
            ceil(v.w)
        };
    }

    // Returns component-wise truncation
    [[nodiscard]] constexpr Vector4 trunc(const Vector4& v) noexcept {
        return {
            trunc(v.x),
            trunc(v.y),
            trunc(v.z),
            trunc(v.w)
        };
    }

    // Returns component-wise round
    [[nodiscard]] constexpr Vector4 round(const Vector4& v) noexcept {
        return {
            round(v.x),
            round(v.y),
            round(v.z),
            round(v.w)
        };
    }

    // Returns component-wise sign. Note that -0 still returns 0
    [[nodiscard]] constexpr Vector4 sign(const Vector4& v) noexcept {
        return {
            sign(v.x),
            sign(v.y),
            sign(v.z),
            sign(v.w)
        };
    }

    // Component-wise approximate equality against an epsilon.
    [[nodiscard]] constexpr bool nearlyEqual(const Vector4& a, const Vector4& b, f32 epsilon = CMP_EPSILON) noexcept {
        return nearlyEqual(a.x, b.x, epsilon) && nearlyEqual(a.y, b.y, epsilon)
            && nearlyEqual(a.z, b.z, epsilon) && nearlyEqual(a.w, b.w, epsilon);
    }
} // namespace draco::math

export namespace std {
    template<> struct formatter<draco::math::Vector4> : formatter<draco::f32> {
        auto format(const draco::math::Vector4& v, format_context& ctx) const {
            ctx.advance_to(format_to(ctx.out(), "{{"));

            for (draco::i32 i = 0; i < 4; ++i) {
                if (i) ctx.advance_to(format_to(ctx.out(), ", "));
                ctx.advance_to(formatter<draco::f32>::format(v[i], ctx));
            }

            return format_to(ctx.out(), "}}");
        }
    };
}
