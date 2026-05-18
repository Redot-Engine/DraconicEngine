module;

#include <cmath>
#include <algorithm>
#include <format>
#include "platform/simd.h"

#if ARCH_X64
    #include <immintrin.h>
#elif ARCH_ARM64
    #include <arm_neon.h>
#endif

export module core.math.types:vector3;
export import :common;

import core.math.constants;
import core.math.functions;
import core.defs;

export namespace draco::math {
    // assertions
    static_assert(sizeof(Vector3) == 16, "Vector3 must be 16 bytes");
    static_assert(alignof(Vector3) == 16, "Vector3 must be 16-byte aligned");
    static_assert(trivial<Vector3>, "Vector3 must be trivial");
    static_assert(std::is_standard_layout_v<Vector3>, "Vector3 must be standard layout");

    // constructors
    [[nodiscard]] constexpr Vector3::Vector3(const float n) noexcept
        : x{n}, y{n}, z{n} { }

    [[nodiscard]] constexpr Vector3::Vector3(const float x, const float y, const float z) noexcept
        : x{x}, y{y}, z{z} { }

    [[nodiscard]] constexpr Vector3::Vector3(const Vector2& xy, const float z) noexcept
        : x{xy.x}, y{xy.y}, z{z} { }

    [[nodiscard]] constexpr Vector3::Vector3(const float x, const Vector2& yz) noexcept
        : x{x}, y{yz.x}, z{yz.y} { }

    [[nodiscard]] constexpr Vector3::Vector3(const Vector4& xyz) noexcept
        : x{xyz.x}, y{xyz.y}, z{xyz.z} { }
    
    // static
    [[nodiscard]] constexpr Vector3 Vector3::x_axis(const float x) noexcept {
        return { x, 0.0f, 0.0f };
    }

    [[nodiscard]] constexpr Vector3 Vector3::y_axis(const float y) noexcept {
        return { 0.0f, y, 0.0f };
    }

    [[nodiscard]] constexpr Vector3 Vector3::z_axis(const float z) noexcept {
        return { 0.0f, 0.0f, z };
    }

    [[nodiscard]] Vector3 Vector3::spherical(const float azimuth, const float inclination, const float radius) noexcept {
        const float sin_incl = radius * std::sin(inclination);
        return { sin_incl * std::cos(azimuth), radius * std::cos(inclination), sin_incl * std::sin(azimuth) };
    }

    [[nodiscard]] Vector3 Vector3::cylindrical(const float angle, const float radius, const float height) noexcept {
        return { radius * std::cos(angle), height, radius * std::sin(angle) };
    }

    // element access
    [[nodiscard]] constexpr float& Vector3::operator[](const int i) noexcept {
        if consteval {
            switch (i) {
                case 0: return x;
                case 1: return y;
                default:
                case 2: return z;
            }
        } else { return (&x)[i]; }
    }

    [[nodiscard]] constexpr const float& Vector3::operator[](const int i) const noexcept {
        if consteval {
            switch (i) {
                case 0: return x;
                case 1: return y;
                default:
                case 2: return z;
            }
        } else { return (&x)[i]; }
    }

    // swizzle
    [[nodiscard]] constexpr Vector2 Vector3::operator[](const int i0, const int i1) noexcept {
        if consteval {
            return { select(i0, x, y, z), select(i1, x, y, z) };
        } else {
            return { (&x)[i0], (&x)[i1] };
        }
    }

    [[nodiscard]] constexpr Vector2 Vector3::operator[](const int i0, const int i1) const noexcept {
        if consteval {
            return { select(i0, x, y, z), select(i1, x, y, z) };
        } else {
            return { (&x)[i0], (&x)[i1] };
        }
    }

    [[nodiscard]] constexpr Vector3 Vector3::operator[](const int i0, const int i1, const int i2) noexcept {
        if consteval {
            return { select(i0, x, y, z), select(i1, x, y, z), select(i2, x, y, z) };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2] };
        }
    }

    [[nodiscard]] constexpr Vector3 Vector3::operator[](const int i0, const int i1, const int i2) const noexcept {
        if consteval {
            return { select(i0, x, y, z), select(i1, x, y, z), select(i2, x, y, z) };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2] };
        }
    }

    [[nodiscard]] constexpr Vector4 Vector3::operator[](const int i0, const int i1, const int i2, const int i3) noexcept {
        if consteval {
            return { select(i0, x, y, z), select(i1, x, y, z), select(i2, x, y, z), select(i3, x, y, z)  };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2], (&x)[i3] };
        }
    }

    [[nodiscard]] constexpr Vector4 Vector3::operator[](const int i0, const int i1, const int i2, const int i3) const noexcept {
        if consteval {
            return { select(i0, x, y, z), select(i1, x, y, z), select(i2, x, y, z), select(i3, x, y, z)  };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2], (&x)[i3] };
        }
    }

    // operators
    constexpr Vector3& Vector3::operator+=(const Vector3& other) noexcept {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    constexpr Vector3& Vector3::operator+=(const float other) noexcept {
        x += other;
        y += other;
        z += other;
        return *this;
    }

    constexpr Vector3& Vector3::operator-=(const Vector3& other) noexcept {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    constexpr Vector3& Vector3::operator-=(const float other) noexcept {
        x -= other;
        y -= other;
        z -= other;
        return *this;
    }

    constexpr Vector3& Vector3::operator*=(const Vector3& other) noexcept {
        x *= other.x;
        y *= other.y;
        z *= other.z;
        return *this;
    }

    constexpr Vector3& Vector3::operator*=(const float other) noexcept {
        x *= other;
        y *= other;
        z *= other;
        return *this;
    }

    constexpr Vector3& Vector3::operator/=(const Vector3& other) noexcept {
        x /= other.x;
        y /= other.y;
        z /= other.z;
        return *this;
    }

    constexpr Vector3& Vector3::operator/=(const float other) noexcept {
        const float inv = 1.0f / other;
        x *= inv;
        y *= inv;
        z *= inv;
        return *this;
    }

    constexpr Vector3& Vector3::operator=(const float other) noexcept {
        x = other;
        y = other;
        z = other;
        return *this;
    }

    [[nodiscard]] constexpr Vector3 Vector3::operator+() const noexcept {
        return { x, y, z };
    }

    [[nodiscard]] constexpr Vector3 Vector3::operator-() const noexcept {
        return { -x, -y, -z };
    }
    
    [[nodiscard]] constexpr Vector3 operator+(const Vector3& a, const Vector3& b) noexcept {
        return { a.x+b.x, a.y+b.y, a.z+b.z };
    }

    [[nodiscard]] constexpr Vector3 operator+(const Vector3& a, const float b) noexcept {
        return { a.x+b, a.y+b, a.z+b };
    }

    [[nodiscard]] constexpr Vector3 operator+(const float a, const Vector3& b) noexcept {
        return b+a;
    }

    [[nodiscard]] constexpr Vector3 operator-(const Vector3& a, const Vector3& b) noexcept {
        return { a.x-b.x, a.y-b.y, a.z-b.z };
    }

    [[nodiscard]] constexpr Vector3 operator-(const Vector3& a, const float b) noexcept {
        return { a.x-b, a.y-b, a.z-b };
    }

    [[nodiscard]] constexpr Vector3 operator-(const float a, const Vector3& b) noexcept {
        return { a-b.x, a-b.y, a-b.z };
    }

    [[nodiscard]] constexpr Vector3 operator*(const Vector3& a, const Vector3& b) noexcept {
        return { a.x*b.x, a.y*b.y, a.z*b.z };
    }

    [[nodiscard]] constexpr Vector3 operator*(const Vector3& a, const float b) noexcept {
        return { a.x*b, a.y*b, a.z*b };
    }

    [[nodiscard]] constexpr Vector3 operator*(const float a, const Vector3& b) noexcept {
        return b*a;
    }

    [[nodiscard]] constexpr Vector3 operator/(const Vector3& a, const Vector3& b) noexcept {
        return { a.x/b.x, a.y/b.y, a.z/b.z };
    }
    
    [[nodiscard]] constexpr Vector3 operator/(const Vector3& a, const float b) noexcept {
        return a * (1.0f / b);
    }
    
    [[nodiscard]] constexpr Vector3 operator/(const float a, const Vector3& b) noexcept {
        return { a/b.x, a/b.y, a/b.z };
    }

    // functions

    // Returns dot product
    [[nodiscard]] constexpr float dot(const Vector3& a, const Vector3& b) noexcept {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    }

    // Returns squared magnitude
    [[nodiscard]] constexpr float length_sq(const Vector3& v) noexcept {
        return dot(v, v);
    }

    // Returns magnitude
    [[nodiscard]] float length(const Vector3& v) noexcept {
        return std::sqrt(length_sq(v));
    }

    // Return squared distance between two vectors
    [[nodiscard]] constexpr float distance_sq(const Vector3& a, const Vector3& b) noexcept {
        return length_sq(a - b);
    }

    // Returns distance between two vectors
    [[nodiscard]] float distance(const Vector3& a, const Vector3& b) noexcept {
        return length(a - b);
    }

    // Safe normalize, checks length
    [[nodiscard]] Vector3 normalize(const Vector3& v) noexcept {
        const float len = length(v);

        return (len > CMP_NORMALIZE_TOLERANCE) ? v / len : Vector3();
    }
    
    // Faster normalize, it presupposes vector has non-zero length
    // TODO: add check that v is non-zero on debug builds
    [[nodiscard]] Vector3 normalize_fast(const Vector3& v) noexcept {
        return v / length(v);
    }

    // Returns vector projected onto normal
    [[nodiscard]] constexpr Vector3 project(const Vector3& vector, const Vector3& normal) noexcept {
        return normal * (dot(vector, normal) / length_sq(normal));
    }

    // Returns a vector reflected off a plane defined by its normal
    [[nodiscard]] constexpr Vector3 reflect(const Vector3& incoming, const Vector3& normal) noexcept {
        return incoming - 2.0f * dot(incoming, normal) * normal;
    }

    // Returns the angle between two vectors
    [[nodiscard]] float angle(const Vector3& a, const Vector3& b) noexcept {
        return std::acos(dot(a, b) / (length(a) * length(b)));
    }

    // Returns linear interpolation between two vectors
    [[nodiscard]] constexpr Vector3 lerp(const Vector3& from, const Vector3& to, const float weight) noexcept {
        return {
            lerp(from.x, to.x, weight),
            lerp(from.y, to.y, weight),
            lerp(from.z, to.z, weight)
        };
    }

    // Returns component-wise minimum
    [[nodiscard]] constexpr Vector3 min(const Vector3& a, const Vector3& b) noexcept {
        return {
            std::min(a.x, b.x),
            std::min(a.y, b.y),
            std::min(a.z, b.z)
        };
    }

    [[nodiscard]] constexpr Vector3 min(const Vector3& a, const float b) noexcept {
        return {
            std::min(a.x, b),
            std::min(a.y, b),
            std::min(a.z, b)
        };
    }

    [[nodiscard]] constexpr Vector3 min(const float a, const Vector3& b) noexcept {
        return min(b, a);
    }

    // Returns the vector with the smaller length
    [[nodiscard]] constexpr Vector3 min_length(const Vector3& a, const Vector3& b) noexcept {
        return length_sq(a) < length_sq(b) ? a : b;
    }

    // Returns a vector in the same direction whose length is bounded above by the given value.
    [[nodiscard]] Vector3 min_length(const Vector3& a, const float b) noexcept {
        const float len_sq = length_sq(a);
        
        if (len_sq > b * b) {
            return a * (b / std::sqrt(len_sq));
        } else {
            return a;
        }
    }

    [[nodiscard]] Vector3 min_length(const float a, const Vector3& b) noexcept {
        return min_length(b, a);
    }

    // Returns component-wise maximum
    [[nodiscard]] constexpr Vector3 max(const Vector3& a, const Vector3& b) noexcept {
        return {
            std::max(a.x, b.x),
            std::max(a.y, b.y),
            std::max(a.z, b.z)
        };
    }

    [[nodiscard]] constexpr Vector3 max(const Vector3& a, const float b) noexcept {
        return {
            std::max(a.x, b),
            std::max(a.y, b),
            std::max(a.z, b)
        };
    }

    [[nodiscard]] constexpr Vector3 max(const float a, const Vector3& b) noexcept {
        return max(b, a);
    }

    // Returns the vector with the larger length
    [[nodiscard]] constexpr Vector3 max_length(const Vector3& a, const Vector3& b) noexcept {
        return length_sq(a) > length_sq(b) ? a : b;
    }

    // Returns a vector in the same direction whose length is bounded below by the given value. Returns the 0 vector if the vector is too small to be normalized.
    [[nodiscard]] Vector3 max_length(const Vector3& a, const float b) noexcept {
        const float len_sq = length_sq(a);
        
        if (len_sq <= CMP_NORMALIZE_TOLERANCE2) {
            return Vector3();
        } else if (len_sq < b * b) {
            return a * (b / std::sqrt(len_sq));
        } else {
            return a;
        }
    }

    [[nodiscard]] Vector3 max_length(const float a, const Vector3& b) noexcept {
        return max_length(b, a);
    }

    // Clamps each component of x to the range [x_min, x_max]. Presupposes x_min <= x_max.
    [[nodiscard]] constexpr Vector3 clamp(const Vector3& x, const Vector3& x_min, const Vector3& x_max) noexcept {
        return max(x_min, min(x, x_max));
    }

    [[nodiscard]] constexpr Vector3 clamp(const Vector3& x, const float x_min, const float x_max) noexcept {
        return max(x_min, min(x, x_max));
    }

    // Clamps the length of the vector to the range [x_min, x_max]. Presupposes x_min <= x_max. Returns the 0 vector if the vector is too small to be normalized.
    [[nodiscard]] Vector3 clamp_length(const Vector3& v, const float x_min, const float x_max) noexcept {
        const float len_sq = length_sq(v);
        
        if (len_sq <= CMP_NORMALIZE_TOLERANCE2) {
            return Vector3();
        } else if (len_sq < x_min * x_min) {
            return v * (x_min / std::sqrt(len_sq));
        } else if (len_sq > x_max * x_max) {
            return v * (x_max / std::sqrt(len_sq));
        } else {
            return v;
        }
    }

    // Returns component-wise absolute value
    [[nodiscard]] constexpr Vector3 abs(const Vector3& v) noexcept {
        return {
            abs(v.x),
            abs(v.y),
            abs(v.z)
        };
    }

    // Returns component-wise floor
    [[nodiscard]] constexpr Vector3 floor(const Vector3& v) noexcept {
        return {
            floor(v.x),
            floor(v.y),
            floor(v.z)
        };
    }

    // Returns component-wise ceiling
    [[nodiscard]] constexpr Vector3 ceil(const Vector3& v) noexcept {
        return {
            ceil(v.x),
            ceil(v.y),
            ceil(v.z)
        };
    }

    // Returns component-wise truncation
    [[nodiscard]] constexpr Vector3 trunc(const Vector3& v) noexcept {
        return {
            trunc(v.x),
            trunc(v.y),
            trunc(v.z)
        };
    }

    // Returns component-wise round
    [[nodiscard]] constexpr Vector3 round(const Vector3& v) noexcept {
        return {
            round(v.x),
            round(v.y),
            round(v.z)
        };
    }

    // Returns component-wise sign. Note that -0 still returns 0
    [[nodiscard]] constexpr Vector3 sign(const Vector3& v) noexcept {
        return {
            sign(v.x),
            sign(v.y),
            sign(v.z)
        };
    }

    // Returns true if the vectors are approximately equal
    [[nodiscard]] constexpr bool approx_eq(const Vector3& a, const Vector3& b) noexcept {
        return distance_sq(a, b) < CMP_EPSILON2;
    }

    // Returns cross product
    [[nodiscard]] constexpr Vector3 cross(const Vector3& a, const Vector3& b) noexcept {
        return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
    }
}

export namespace std {
    template<> struct formatter<draco::math::Vector3> : formatter<float> {
        auto format(const draco::math::Vector3& v, format_context& ctx) const {
            ctx.advance_to(format_to(ctx.out(), "{{"));

            for (int i = 0; i < 3; ++i) {
                if (i) ctx.advance_to(format_to(ctx.out(), ", "));
                ctx.advance_to(formatter<float>::format(v[i], ctx));
            }

            return format_to(ctx.out(), "}}");
        }
    };
}