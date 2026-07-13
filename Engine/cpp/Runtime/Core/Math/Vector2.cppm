module;

#include <cmath>
#include <format>

export module core.math.types:vector2;
export import :common;

import core.math.constants;
import core.math.functions;
import core.defs;
import core.stdtypes;

export namespace draco::math {
    // assertions
    static_assert(sizeof(Vector2) == 8, "Vector2 must be 8 bytes");
    static_assert(alignof(Vector2) == 8, "Vector2 must be 8-byte aligned");
    static_assert(trivial<Vector2>, "Vector2 must be trivial");
    static_assert(std::is_standard_layout_v<Vector2>, "Vector2 must be standard layout");

    // constructors
    [[nodiscard]] constexpr Vector2::Vector2(const f32 n) noexcept
        : x{n}, y{n} { }
    
    [[nodiscard]] constexpr Vector2::Vector2(const f32 x, const f32 y) noexcept
        : x{x}, y{y} { }
        
    [[nodiscard]] constexpr Vector2::Vector2(const Vector3& xy) noexcept
        : x{xy.x}, y{xy.y} { }

    [[nodiscard]] constexpr Vector2::Vector2(const Vector4& xy) noexcept
        : x{xy.x}, y{xy.y} { }
    
    // static
    [[nodiscard]] constexpr Vector2 Vector2::xAxis(const f32 x) noexcept {
        return { x, 0.0f };
    }

    [[nodiscard]] constexpr Vector2 Vector2::yAxis(const f32 y) noexcept {
        return { 0.0f, y };
    }

    [[nodiscard]] Vector2 Vector2::polar(const f32 angle, const f32 radius) noexcept {
        return { radius * std::cos(angle), radius * std::sin(angle) };
    }
    
    // element access
    [[nodiscard]] constexpr f32& Vector2::operator[](const i32 i) noexcept {
        if consteval {
            return select(i, x, y);
        } else {
            return (&x)[i];
        }
    }

    [[nodiscard]] constexpr const f32& Vector2::operator[](const i32 i) const noexcept {
        if consteval {
            return select(i, x, y);
        } else {
            return (&x)[i];
        }
    }

    // swizzle
    [[nodiscard]] constexpr Vector2 Vector2::operator[](const i32 i0, const i32 i1) const noexcept {
        if consteval {
            return { select(i0, x, y), select(i1, x, y) };
        } else {
            return { (&x)[i0], (&x)[i1] };
        }
    }

    [[nodiscard]] constexpr Vector3 Vector2::operator[](const i32 i0, const i32 i1, const i32 i2) const noexcept {
        if consteval {
            return { select(i0, x, y), select(i1, x, y), select(i2, x, y) };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2] };
        }
    }

    [[nodiscard]] constexpr Vector4 Vector2::operator[](const i32 i0, const i32 i1, const i32 i2, const i32 i3) const noexcept {
        if consteval {
            return { select(i0, x, y), select(i1, x, y), select(i2, x, y), select(i3, x, y) };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2], (&x)[i3] };
        }
    }

    // operators
    constexpr Vector2& Vector2::operator+=(const Vector2& other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr Vector2& Vector2::operator+=(const f32 other) noexcept {
        x += other;
        y += other;
        return *this;
    }

    constexpr Vector2& Vector2::operator-=(const Vector2& other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr Vector2& Vector2::operator-=(const f32 other) noexcept {
        x -= other;
        y -= other;
        return *this;
    }

    constexpr Vector2& Vector2::operator*=(const Vector2& other) noexcept {
        x *= other.x;
        y *= other.y;
        return *this;
    }

    constexpr Vector2& Vector2::operator*=(const f32 other) noexcept {
        x *= other;
        y *= other;
        return *this;
    }

    constexpr Vector2& Vector2::operator/=(const Vector2& other) noexcept {
        x /= other.x;
        y /= other.y;
        return *this;
    }

    constexpr Vector2& Vector2::operator/=(const f32 other) noexcept {
        const f32 inv = 1.0f / other;
        x *= inv;
        y *= inv;
        return *this;
    }

    constexpr Vector2& Vector2::operator=(const f32 other) noexcept {
        x = other;
        y = other;
        return *this;
    }

    [[nodiscard]] constexpr Vector2 Vector2::operator+() const noexcept {
        return { x, y };
    }

    [[nodiscard]] constexpr Vector2 Vector2::operator-() const noexcept {
        return { -x, -y };
    }
    
    [[nodiscard]] constexpr Vector2 operator+(const Vector2& a, const Vector2& b) noexcept {
        return { a.x+b.x, a.y+b.y };
    }

    [[nodiscard]] constexpr Vector2 operator+(const Vector2& a, const f32 b) noexcept {
        return { a.x+b, a.y+b };
    }

    [[nodiscard]] constexpr Vector2 operator+(const f32 a, const Vector2& b) noexcept {
        return b+a;
    }

    [[nodiscard]] constexpr Vector2 operator-(const Vector2& a, const Vector2& b) noexcept {
        return { a.x-b.x, a.y-b.y };
    }

    [[nodiscard]] constexpr Vector2 operator-(const Vector2& a, const f32 b) noexcept {
        return { a.x-b, a.y-b };
    }

    [[nodiscard]] constexpr Vector2 operator-(const f32 a, const Vector2& b) noexcept {
        return { a-b.x, a-b.y };
    }

    [[nodiscard]] constexpr Vector2 operator*(const Vector2& a, const Vector2& b) noexcept {
        return { a.x*b.x, a.y*b.y };
    }

    [[nodiscard]] constexpr Vector2 operator*(const Vector2& a, const f32 b) noexcept {
        return { a.x*b, a.y*b };
    }

    [[nodiscard]] constexpr Vector2 operator*(const f32 a, const Vector2& b) noexcept {
        return b*a;
    }

    [[nodiscard]] constexpr Vector2 operator/(const Vector2& a, const Vector2& b) noexcept {
        return { a.x/b.x, a.y/b.y };
    }
    
    [[nodiscard]] constexpr Vector2 operator/(const Vector2& a, const f32 b) noexcept {
        return a * (1.0f / b);
    }
    
    [[nodiscard]] constexpr Vector2 operator/(const f32 a, const Vector2& b) noexcept {
        return { a/b.x, a/b.y };
    }

    // functions

    // Returns dot product
    [[nodiscard]] constexpr f32 dot(const Vector2& a, const Vector2& b) noexcept {
        return a.x*b.x + a.y*b.y;
    }

    // Returns squared magnitude
    [[nodiscard]] constexpr f32 length_sq(const Vector2& v) noexcept {
        return dot(v, v);
    }

    // Returns magnitude
    [[nodiscard]] f32 length(const Vector2& v) noexcept {
        return std::sqrt(length_sq(v));
    }

    // Return squared distance between two vectors
    [[nodiscard]] constexpr f32 distance_sq(const Vector2& a, const Vector2& b) noexcept {
        return length_sq(a - b);
    }

    // Returns distance between two vectors
    [[nodiscard]] f32 distance(const Vector2& a, const Vector2& b) noexcept {
        return length(a - b);
    }

    // Safe normalize, checks length
    [[nodiscard]] Vector2 normalize(const Vector2& v) noexcept {
        const f32 len = length(v);

        return (len > CMP_NORMALIZE_TOLERANCE) ? v / len : Vector2();
    }
    
    // Faster normalize, it presupposes vector has non-zero length
    // TODO: add check that v is non-zero on debug builds
    [[nodiscard]] Vector2 normalize_fast(const Vector2& v) noexcept {
        return v / length(v);
    }

    // Returns vector projected onto normal
    [[nodiscard]] constexpr Vector2 project(const Vector2& vector, const Vector2& normal) noexcept {
        return normal * (dot(vector, normal) / length_sq(normal));
    }

    // Returns a vector reflected off a plane defined by its normal
    [[nodiscard]] constexpr Vector2 reflect(const Vector2& incoming, const Vector2& normal) noexcept {
        return incoming - 2.0f * dot(incoming, normal) * normal;
    }

    // Returns the angle between two vectors
    [[nodiscard]] f32 angle(const Vector2& a, const Vector2& b) noexcept {
        return std::acos(dot(a, b) / (length(a) * length(b)));
    }

    // Returns linear i32erpolation between two vectors
    [[nodiscard]] constexpr Vector2 lerp(const Vector2& from, const Vector2& to, const f32 weight) noexcept {
        return {
            lerp(from.x, to.x, weight),
            lerp(from.y, to.y, weight)
        };
    }

    // Returns component-wise minimum
    [[nodiscard]] constexpr Vector2 min(const Vector2& a, const Vector2& b) noexcept {
        return {
            std::min(a.x, b.x),
            std::min(a.y, b.y)
        };
    }

    [[nodiscard]] constexpr Vector2 min(const Vector2& a, const f32 b) noexcept {
        return {
            std::min(a.x, b),
            std::min(a.y, b)
        };
    }

    [[nodiscard]] constexpr Vector2 min(const f32 a, const Vector2& b) noexcept {
        return min(b, a);
    }

    // Returns the vector with the smaller length
    [[nodiscard]] constexpr Vector2 min_length(const Vector2& a, const Vector2& b) noexcept {
        return length_sq(a) < length_sq(b) ? a : b;
    }

    // Returns a vector in the same direction whose length is bounded above by the given value.
    [[nodiscard]] Vector2 min_length(const Vector2& a, const f32 b) noexcept {
        const f32 len_sq = length_sq(a);
        
        if (len_sq > b * b) {
            return a * (b / std::sqrt(len_sq));
        } else {
            return a;
        }
    }

    [[nodiscard]] Vector2 min_length(const f32 a, const Vector2& b) noexcept {
        return min_length(b, a);
    }

    // Returns component-wise maximum
    [[nodiscard]] constexpr Vector2 max(const Vector2& a, const Vector2& b) noexcept {
        return {
            std::max(a.x, b.x),
            std::max(a.y, b.y)
        };
    }

    [[nodiscard]] constexpr Vector2 max(const Vector2& a, const f32 b) noexcept {
        return {
            std::max(a.x, b),
            std::max(a.y, b)
        };
    }

    [[nodiscard]] constexpr Vector2 max(const f32 a, const Vector2& b) noexcept {
        return max(b, a);
    }

    // Returns the vector with the larger length
    [[nodiscard]] constexpr Vector2 max_length(const Vector2& a, const Vector2& b) noexcept {
        return length_sq(a) > length_sq(b) ? a : b;
    }

    // Returns a vector in the same direction whose length is bounded below by the given value. Returns the 0 vector if the vector is too small to be normalized.
    [[nodiscard]] Vector2 max_length(const Vector2& a, const f32 b) noexcept {
        const f32 len_sq = length_sq(a);
        
        if (len_sq <= CMP_NORMALIZE_TOLERANCE2) {
            return Vector2();
        } else if (len_sq < b * b) {
            return a * (b / std::sqrt(len_sq));
        } else {
            return a;
        }
    }

    [[nodiscard]] Vector2 max_length(const f32 a, const Vector2& b) noexcept {
        return max_length(b, a);
    }

    // Clamps each component of x to the range [x_min, x_max]. Presupposes x_min <= x_max.
    [[nodiscard]] constexpr Vector2 clamp(const Vector2& x, const Vector2& x_min, const Vector2& x_max) noexcept {
        return max(x_min, min(x, x_max));
    }

    [[nodiscard]] constexpr Vector2 clamp(const Vector2& x, const f32 x_min, const f32 x_max) noexcept {
        return max(x_min, min(x, x_max));
    }

    // Clamps the length of the vector to the range [x_min, x_max]. Presupposes x_min <= x_max. Returns the 0 vector if the vector is too small to be normalized.
    [[nodiscard]] Vector2 clamp_length(const Vector2& v, const f32 x_min, const f32 x_max) noexcept {
        const f32 len_sq = length_sq(v);
        
        if (len_sq <= CMP_NORMALIZE_TOLERANCE2) {
            return Vector2();
        } else if (len_sq < x_min * x_min) {
            return v * (x_min / std::sqrt(len_sq));
        } else if (len_sq > x_max * x_max) {
            return v * (x_max / std::sqrt(len_sq));
        } else {
            return v;
        }
    }

    // Returns component-wise absolute value
    [[nodiscard]] constexpr Vector2 abs(const Vector2& v) noexcept {
        return {
            abs(v.x),
            abs(v.y)
        };
    }

    // Returns component-wise floor
    [[nodiscard]] constexpr Vector2 floor(const Vector2& v) noexcept {
        return {
            floor(v.x),
            floor(v.y)
        };
    }

    // Returns component-wise ceiling
    [[nodiscard]] constexpr Vector2 ceil(const Vector2& v) noexcept {
        return {
            ceil(v.x),
            ceil(v.y)
        };
    }

    // Returns component-wise truncation
    [[nodiscard]] constexpr Vector2 trunc(const Vector2& v) noexcept {
        return {
            trunc(v.x),
            trunc(v.y)
        };
    }

    // Returns component-wise round
    [[nodiscard]] constexpr Vector2 round(const Vector2& v) noexcept {
        return {
            round(v.x),
            round(v.y)
        };
    }

    // Returns component-wise sign. Note that -0 still returns 0
    [[nodiscard]] constexpr Vector2 sign(const Vector2& v) noexcept {
        return {
            sign(v.x),
            sign(v.y)
        };
    }

    // Component-wise approximate equality against an epsilon.
    [[nodiscard]] constexpr bool nearlyEqual(const Vector2& a, const Vector2& b, f32 epsilon = CMP_EPSILON) noexcept {
        return nearlyEqual(a.x, b.x, epsilon) && nearlyEqual(a.y, b.y, epsilon);
    }
}

export namespace std {
    template<> struct formatter<draco::math::Vector2> : formatter<draco::f32> {
        auto format(const draco::math::Vector2& v, format_context& ctx) const {
            ctx.advance_to(format_to(ctx.out(), "{{"));
            ctx.advance_to(formatter<draco::f32>::format(v.x, ctx));
            ctx.advance_to(format_to(ctx.out(), ", "));
            ctx.advance_to(formatter<draco::f32>::format(v.y, ctx));
            return format_to(ctx.out(), "}}");
        }
    };
}