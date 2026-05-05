module;

#include "platform/simd.h"

#if ARCH_X64
    #include <immintrin.h>
#elif ARCH_ARM64
    #include <arm_neon.h>
#endif

module core.math.types;
import std;
import core.math.constants;
import core.defs;

namespace draco::math {
    // constructors
    [[nodiscard]] constexpr Vector2::Vector2(const float n) noexcept
        : x{n}, y{n} { }
    
    [[nodiscard]] constexpr Vector2::Vector2(const float x, const float y) noexcept
        : x{x}, y{y} { }
        
    [[nodiscard]] constexpr Vector2::Vector2(const Vector3& xy) noexcept
        : x{xy.x}, y{xy.y} { }

    [[nodiscard]] constexpr Vector2::Vector2(const Vector4& xy) noexcept
        : x{xy.x}, y{xy.y} { }
    
    // static
    [[nodiscard]] constexpr Vector2 Vector2::x_axis(const float x) noexcept {
        return { x, 0.0f };
    }

    [[nodiscard]] constexpr Vector2 Vector2::y_axis(const float y) noexcept {
        return { 0.0f, y };
    }

    [[nodiscard]] Vector2 Vector2::polar(const float angle, const float radius) noexcept {
        return { radius * std::cos(angle), radius * std::sin(angle) };
    }
    
    // element access
    [[nodiscard]] constexpr float& Vector2::operator[](const int i) noexcept {
        if consteval {
            return i ? y : x;
        } else {
            return (&x)[i];
        }
    }

    [[nodiscard]] constexpr const float& Vector2::operator[](const int i) const noexcept {
        if consteval {
            return i ? y : x;
        } else {
            return (&x)[i];
        }
    }

    // swizzle
    [[nodiscard]] constexpr Vector2 Vector2::operator[](const int i0, const int i1) noexcept {
        if consteval {
            return { i0 ? y : x, i1 ? y : x };
        } else {
            return { (&x)[i0], (&x)[i1] };
        }
    }

    [[nodiscard]] constexpr Vector2 Vector2::operator[](const int i0, const int i1) const noexcept {
        if consteval {
            return { i0 ? y : x, i1 ? y : x };
        } else {
            return { (&x)[i0], (&x)[i1] };
        }
    }

    [[nodiscard]] constexpr Vector3 Vector2::operator[](const int i0, const int i1, const int i2) noexcept {
        if consteval {
            return { i0 ? y : x, i1 ? y : x, i2 ? y : x };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2] };
        }
    }

    [[nodiscard]] constexpr Vector3 Vector2::operator[](const int i0, const int i1, const int i2) const noexcept {
        if consteval {
            return { i0 ? y : x, i1 ? y : x, i2 ? y : x };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2] };
        }
    }

    [[nodiscard]] constexpr Vector4 Vector2::operator[](const int i0, const int i1, const int i2, const int i3) noexcept {
        if consteval {
            return { i0 ? y : x, i1 ? y : x, i2 ? y : x, i3 ? y : x };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2], (&x)[i3] };
        }
    }

    [[nodiscard]] constexpr Vector4 Vector2::operator[](const int i0, const int i1, const int i2, const int i3) const noexcept {
        if consteval {
            return { i0 ? y : x, i1 ? y : x, i2 ? y : x, i3 ? y : x };
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

    constexpr Vector2& Vector2::operator+=(const float other) noexcept {
        x += other;
        y += other;
        return *this;
    }

    constexpr Vector2& Vector2::operator-=(const Vector2& other) noexcept {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr Vector2& Vector2::operator-=(const float other) noexcept {
        x -= other;
        y -= other;
        return *this;
    }

    constexpr Vector2& Vector2::operator*=(const Vector2& other) noexcept {
        x *= other.x;
        y *= other.y;
        return *this;
    }

    constexpr Vector2& Vector2::operator*=(const float other) noexcept {
        x *= other;
        y *= other;
        return *this;
    }

    constexpr Vector2& Vector2::operator/=(const Vector2& other) noexcept {
        x /= other.x;
        y /= other.y;
        return *this;
    }

    constexpr Vector2& Vector2::operator/=(const float other) noexcept {
        const float inv = 1.0f / other;
        x *= inv;
        y *= inv;
        return *this;
    }

    constexpr Vector2& Vector2::operator=(const float other) noexcept {
        x = other;
        y = other;
        return *this;
    }

    [[nodiscard]] constexpr Vector2 Vector2::operator+() noexcept {
        return { x, y };
    }

    [[nodiscard]] constexpr Vector2 Vector2::operator-() noexcept {
        return { -x, -y };
    }
    
    [[nodiscard]] constexpr Vector2 operator+(const Vector2& a, const Vector2& b) noexcept {
        return { a.x+b.x, a.y+b.y };
    }

    [[nodiscard]] constexpr Vector2 operator+(const Vector2& a, const float b) noexcept {
        return { a.x+b, a.y+b };
    }

    [[nodiscard]] constexpr Vector2 operator+(const float a, const Vector2& b) noexcept {
        return b+a;
    }

    [[nodiscard]] constexpr Vector2 operator-(const Vector2& a, const Vector2& b) noexcept {
        return { a.x-b.x, a.y-b.y };
    }

    [[nodiscard]] constexpr Vector2 operator-(const Vector2& a, const float b) noexcept {
        return { a.x-b, a.y-b };
    }

    [[nodiscard]] constexpr Vector2 operator-(const float a, const Vector2& b) noexcept {
        return { a-b.x, a-b.y };
    }

    [[nodiscard]] constexpr Vector2 operator*(const Vector2& a, const Vector2& b) noexcept {
        return { a.x*b.x, a.y*b.y };
    }

    [[nodiscard]] constexpr Vector2 operator*(const Vector2& a, const float b) noexcept {
        return { a.x*b, a.y*b };
    }

    [[nodiscard]] constexpr Vector2 operator*(const float a, const Vector2& b) noexcept {
        return b*a;
    }

    [[nodiscard]] constexpr Vector2 operator/(const Vector2& a, const Vector2& b) noexcept {
        return { a.x/b.x, a.y/b.y };
    }
    
    [[nodiscard]] constexpr Vector2 operator/(const Vector2& a, const float b) noexcept {
        return a * (1.0f / b);
    }
    
    [[nodiscard]] constexpr Vector2 operator/(const float a, const Vector2& b) noexcept {
        return { a/b.x, a/b.y };
    }

    // functions
    [[nodiscard]] constexpr float dot(const Vector2& a, const Vector2& b) noexcept {
        return a.x*b.x + a.y*b.y;
    }

    [[nodiscard]] constexpr float length_sq(const Vector2& v) noexcept {
        return dot(v, v);
    }

    [[nodiscard]] float length(const Vector2& v) noexcept {
        return std::sqrt(length_sq(v));
    }

    [[nodiscard]] constexpr float distance_sq(const Vector2& a, const Vector2& b) noexcept {
        return length_sq(a - b);
    }

    [[nodiscard]] float distance(const Vector2& a, const Vector2& b) noexcept {
        return length(a - b);
    }

    [[nodiscard]] Vector2 normalize(const Vector2& v) noexcept {
        const float len = length(v);

        return (len > CMP_NORMALIZE_TOLERANCE<float>) ? v / len : Vector2(0.0f);
    }
    
    [[nodiscard]] Vector2 normalize_fast(const Vector2& v) noexcept {
        return v / length(v);
    }

    [[nodiscard]] constexpr Vector2 project(const Vector2& vector, const Vector2& normal) noexcept {
        return normal * (dot(vector, normal) / length_sq(normal));
    }

    [[nodiscard]] constexpr Vector2 reflect(const Vector2& incoming, const Vector2& normal) noexcept {
        return incoming - 2.0f * dot(incoming, normal) * normal;
    }

    [[nodiscard]] float angle(const Vector2& a, const Vector2& b) noexcept {
        return std::acos(dot(a, b) / (length(a) * length(b)));
    }

    [[nodiscard]] constexpr Vector2 lerp(const Vector2& from, const Vector2& to, const float weight) noexcept {
        return {
            std::lerp(from.x, to.x, weight),
            std::lerp(from.y, to.y, weight)
        };
    }

    [[nodiscard]] constexpr Vector2 min(const Vector2& a, const Vector2& b) noexcept {
        return {
            std::min(a.x, b.x),
            std::min(a.y, b.y)
        };
    }

    [[nodiscard]] constexpr Vector2 min(const Vector2& a, const float b) noexcept {
        return {
            std::min(a.x, b),
            std::min(a.y, b)
        };
    }

    [[nodiscard]] constexpr Vector2 min(const float a, const Vector2& b) noexcept {
        return min(b, a);
    }

    [[nodiscard]] constexpr Vector2 min_length(const Vector2& a, const Vector2& b) noexcept {
        return length_sq(a) < length_sq(b) ? a : b;
    }

    [[nodiscard]] Vector2 min_length(const Vector2& a, const float b) noexcept {
        const float len_sq = length_sq(a);
        
        if (len_sq > b * b) {
            return a * (b / std::sqrt(len_sq));
        } else {
            return a;
        }
    }

    [[nodiscard]] Vector2 min_length(const float a, const Vector2& b) noexcept {
        return min_length(b, a);
    }

    [[nodiscard]] constexpr Vector2 max(const Vector2& a, const Vector2& b) noexcept {
        return {
            std::max(a.x, b.x),
            std::max(a.y, b.y)
        };
    }

    [[nodiscard]] constexpr Vector2 max(const Vector2& a, const float b) noexcept {
        return {
            std::max(a.x, b),
            std::max(a.y, b)
        };
    }

    [[nodiscard]] constexpr Vector2 max(const float a, const Vector2& b) noexcept {
        return max(b, a);
    }

    [[nodiscard]] constexpr Vector2 max_length(const Vector2& a, const Vector2& b) noexcept {
        return length_sq(a) > length_sq(b) ? a : b;
    }

    [[nodiscard]] Vector2 max_length(const Vector2& a, const float b) noexcept {
        const float len_sq = length_sq(a);
        
        if (len_sq < b * b) {
            return a * (b / std::sqrt(len_sq));
        } else {
            return a;
        }
    }

    [[nodiscard]] Vector2 max_length(const float a, const Vector2& b) noexcept {
        return max_length(b, a);
    }

    [[nodiscard]] constexpr Vector2 clamp(const Vector2& x, const Vector2& x_min, const Vector2& x_max) noexcept {
        return max(x_min, min(x, x_max));
    }

    [[nodiscard]] constexpr Vector2 clamp(const Vector2& x, const float x_min, const float x_max) noexcept {
        return max(x_min, min(x, x_max));
    }

    [[nodiscard]] Vector2 clamp_length(const Vector2& v, const float x_min, const float x_max) noexcept {
        const float len_sq = length_sq(v);
        
        if (len_sq < x_min * x_min) {
            return v * (x_min / std::sqrt(len_sq));
        } else if (len_sq > x_max * x_max) {
            return v * (x_max / std::sqrt(len_sq));
        } else {
            return v;
        }
    }

    [[nodiscard]] constexpr Vector2 abs(const Vector2& v) noexcept {
        return {
            std::abs(v.x),
            std::abs(v.y)
        };
    }

    [[nodiscard]] constexpr Vector2 floor(const Vector2& v) noexcept {
        return {
            std::floor(v.x),
            std::floor(v.y)
        };
    }

    [[nodiscard]] constexpr Vector2 ceil(const Vector2& v) noexcept {
        return {
            std::ceil(v.x),
            std::ceil(v.y)
        };
    }

    [[nodiscard]] constexpr Vector2 round(const Vector2& v) noexcept {
        return {
            std::round(v.x),
            std::round(v.y)
        };
    }

    [[nodiscard]] constexpr Vector2 sign(const Vector2& v) noexcept {
        return {
            v.x == 0.0f ? 0.0f : v.x > 0.0f ? 1.0f : -1.0f,
            v.y == 0.0f ? 0.0f : v.y > 0.0f ? 1.0f : -1.0f
        };
    }

    [[nodiscard]] constexpr bool approx_eq(const Vector2& a, const Vector2& b) noexcept {
        return distance_sq(a, b) < CMP_EPSILON2<float>;
    }
}

namespace std {
    template<> struct formatter<draco::math::Vector2> : std::formatter<float> {
        auto format(const draco::math::Vector2& v, std::format_context& ctx) const {
            ctx.advance_to(std::format_to(ctx.out(), "{{"));
            ctx.advance_to(std::formatter<float>::format(v.x, ctx));
            ctx.advance_to(std::format_to(ctx.out(), ", "));
            ctx.advance_to(std::formatter<float>::format(v.y, ctx));
            return std::format_to(ctx.out(), "}}");
        }
    };
}