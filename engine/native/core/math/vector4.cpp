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
    [[nodiscard]] constexpr Vector4::Vector4(const float n) noexcept
        : x{n}, y{n}, z{n}, w{n} { }

    [[nodiscard]] constexpr Vector4::Vector4(const float x, const float y, const float z, const float w) noexcept
        : x{x}, y{y}, z{z}, w{w} { }

    [[nodiscard]] constexpr Vector4::Vector4(const Vector2& xy) noexcept
        : x{xy.x}, y{xy.y}, z{0.0f}, w{0.0f} { }

    [[nodiscard]] constexpr Vector4::Vector4(const Vector2& xy, const float z, const float w) noexcept
        : x{xy.x}, y{xy.y}, z{z}, w{w} { }

    [[nodiscard]] constexpr Vector4::Vector4(const float x, const Vector2& yz, const float w) noexcept
        : x{x}, y{yz.x}, z{yz.y}, w{w} { }

    [[nodiscard]] constexpr Vector4::Vector4(const float x, const float y, const Vector2& zw) noexcept
        : x{x}, y{y}, z{zw.x}, w{zw.y} { }

    [[nodiscard]] constexpr Vector4::Vector4(const Vector2& xy, const Vector2& zw) noexcept
        : x{xy.x}, y{xy.y}, z{zw.x}, w{zw.y} { }

    [[nodiscard]] constexpr Vector4::Vector4(const Vector3& xyz, const float w) noexcept
        : x{xyz.x}, y{xyz.y}, z{xyz.z}, w{w} { }

    [[nodiscard]] constexpr Vector4::Vector4(const float x, const Vector3& yzw) noexcept
        : x{x}, y{yzw.x}, z{yzw.y}, w{yzw.z} { }
    
    // static
    [[nodiscard]] constexpr Vector4 Vector4::x_axis(const float x) noexcept {
        return { x, 0.0f, 0.0f, 0.0f };
    }

    [[nodiscard]] constexpr Vector4 Vector4::y_axis(const float y) noexcept {
        return { 0.0f, y, 0.0f, 0.0f };
    }

    [[nodiscard]] constexpr Vector4 Vector4::z_axis(const float z) noexcept {
        return { 0.0f, 0.0f, z, 0.0f };
    }

    [[nodiscard]] constexpr Vector4 Vector4::w_axis(const float w) noexcept {
        return { 0.0f, 0.0f, 0.0f, w };
    }

    // element access
    [[nodiscard]] constexpr float& Vector4::operator[](const int i) noexcept {
        if consteval {
            switch (i) {
                case 0: return x;
                case 1: return y;
                case 2: return z;
                default:
                case 3: return w;
            }
        } else { return (&x)[i]; }
    }

    [[nodiscard]] constexpr const float& Vector4::operator[](const int i) const noexcept {
        if consteval {
            switch (i) {
                case 0: return x;
                case 1: return y;
                case 2: return z;
                default:
                case 3: return w;
            }
        } else { return (&x)[i]; }
    }

    // swizzle
    [[nodiscard]] constexpr Vector2 Vector4::operator[](const int i0, const int i1) noexcept {
        if consteval {
            return { select(i0, x, y, z, w), select(i1, x, y, z, w) };
        } else {
            return { (&x)[i0], (&x)[i1] };
        }
    }

    [[nodiscard]] constexpr Vector2 Vector4::operator[](const int i0, const int i1) const noexcept {
        if consteval {
            return { select(i0, x, y, z, w), select(i1, x, y, z, w) };
        } else {
            return { (&x)[i0], (&x)[i1] };
        }
    }

    [[nodiscard]] constexpr Vector3 Vector4::operator[](const int i0, const int i1, const int i2) noexcept {
        if consteval {
            return { select(i0, x, y, z, w), select(i1, x, y, z, w), select(i2, x, y, z, w) };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2] };
        }
    }

    [[nodiscard]] constexpr Vector3 Vector4::operator[](const int i0, const int i1, const int i2) const noexcept {
        if consteval {
            return { select(i0, x, y, z, w), select(i1, x, y, z, w), select(i2, x, y, z, w) };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2] };
        }
    }

    [[nodiscard]] constexpr Vector4 Vector4::operator[](const int i0, const int i1, const int i2, const int i3) noexcept {
        if consteval {
            return { select(i0, x, y, z, w), select(i1, x, y, z, w), select(i2, x, y, z, w), select(i3, x, y, z, w)  };
        } else {
            return { (&x)[i0], (&x)[i1], (&x)[i2], (&x)[i3] };
        }
    }

    [[nodiscard]] constexpr Vector4 Vector4::operator[](const int i0, const int i1, const int i2, const int i3) const noexcept {
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

    constexpr Vector4& Vector4::operator+=(const float other) noexcept {
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

    constexpr Vector4& Vector4::operator-=(const float other) noexcept {
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

    constexpr Vector4& Vector4::operator*=(const float other) noexcept {
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

    constexpr Vector4& Vector4::operator/=(const float other) noexcept {
        const float inv = 1.0f / other;
        x *= inv;
        y *= inv;
        z *= inv;
        w *= inv;
        return *this;
    }

    constexpr Vector4& Vector4::operator=(const float other) noexcept {
        x = other;
        y = other;
        z = other;
        w = other;
        return *this;
    }

    [[nodiscard]] constexpr Vector4 Vector4::operator+() noexcept {
        return { x, y, z, w };
    }

    [[nodiscard]] constexpr Vector4 Vector4::operator-() noexcept {
        return { -x, -y, -z, -w };
    }
    
    [[nodiscard]] constexpr Vector4 operator+(const Vector4& a, const Vector4& b) noexcept {
        return { a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w };
    }

    [[nodiscard]] constexpr Vector4 operator+(const Vector4& a, const float b) noexcept {
        return { a.x+b, a.y+b, a.z+b, a.w+b };
    }

    [[nodiscard]] constexpr Vector4 operator+(const float a, const Vector4& b) noexcept {
        return b+a;
    }

    [[nodiscard]] constexpr Vector4 operator-(const Vector4& a, const Vector4& b) noexcept {
        return { a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w };
    }

    [[nodiscard]] constexpr Vector4 operator-(const Vector4& a, const float b) noexcept {
        return { a.x-b, a.y-b, a.z-b, a.w-b };
    }

    [[nodiscard]] constexpr Vector4 operator-(const float a, const Vector4& b) noexcept {
        return { a-b.x, a-b.y, a-b.z, a-b.w };
    }

    [[nodiscard]] constexpr Vector4 operator*(const Vector4& a, const Vector4& b) noexcept {
        return { a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w };
    }

    [[nodiscard]] constexpr Vector4 operator*(const Vector4& a, const float b) noexcept {
        return { a.x*b, a.y*b, a.z*b, a.w*b };
    }

    [[nodiscard]] constexpr Vector4 operator*(const float a, const Vector4& b) noexcept {
        return b*a;
    }

    [[nodiscard]] constexpr Vector4 operator/(const Vector4& a, const Vector4& b) noexcept {
        return { a.x/b.x, a.y/b.y, a.z/b.z, a.w/b.w };
    }
    
    [[nodiscard]] constexpr Vector4 operator/(const Vector4& a, const float b) noexcept {
        return a * (1.0f / b);
    }
    
    [[nodiscard]] constexpr Vector4 operator/(const float a, const Vector4& b) noexcept {
        return { a/b.x, a/b.y, a/b.z, a/b.w };
    }

    // functions
    [[nodiscard]] constexpr float dot(const Vector4& a, const Vector4& b) noexcept {
        if consteval {
            return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
        } else {
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
            #else
                // scalar.
                return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
            #endif
        }
    }

    [[nodiscard]] constexpr float length_sq(const Vector4& v) noexcept {
        return dot(v, v);
    }

    [[nodiscard]] float length(const Vector4& v) noexcept {
        return std::sqrt(length_sq(v));
    }

    [[nodiscard]] constexpr float distance_sq(const Vector4& a, const Vector4& b) noexcept {
        return length_sq(a - b);
    }

    [[nodiscard]] float distance(const Vector4& a, const Vector4& b) noexcept {
        return length(a - b);
    }

    [[nodiscard]] Vector4 normalize(const Vector4& v) noexcept {
        const float len = length(v);

        return (len > CMP_NORMALIZE_TOLERANCE<float>) ? v / len : Vector4(0.0f);
    }
    
    [[nodiscard]] Vector4 normalize_fast(const Vector4& v) noexcept {
        return v / length(v);
    }

    [[nodiscard]] constexpr Vector4 project(const Vector4& vector, const Vector4& normal) noexcept {
        return normal * (dot(vector, normal) / length_sq(normal));
    }

    [[nodiscard]] constexpr Vector4 reflect(const Vector4& incoming, const Vector4& normal) noexcept {
        return incoming - 2.0f * dot(incoming, normal) * normal;
    }

    [[nodiscard]] float angle(const Vector4& a, const Vector4& b) noexcept {
        return std::acos(dot(a, b) / (length(a) * length(b)));
    }

    [[nodiscard]] constexpr Vector4 lerp(const Vector4& from, const Vector4& to, const float weight) noexcept {
        return {
            std::lerp(from.x, to.x, weight),
            std::lerp(from.y, to.y, weight),
            std::lerp(from.z, to.z, weight),
            std::lerp(from.w, to.w, weight)
        };
    }

    [[nodiscard]] constexpr Vector4 min(const Vector4& a, const Vector4& b) noexcept {
        return {
            std::min(a.x, b.x),
            std::min(a.y, b.y),
            std::min(a.z, b.z),
            std::min(a.w, b.w)
        };
    }

    [[nodiscard]] constexpr Vector4 min(const Vector4& a, const float b) noexcept {
        return {
            std::min(a.x, b),
            std::min(a.y, b),
            std::min(a.z, b),
            std::min(a.w, b)
        };
    }

    [[nodiscard]] constexpr Vector4 min(const float a, const Vector4& b) noexcept {
        return min(b, a);
    }

    [[nodiscard]] constexpr Vector4 min_length(const Vector4& a, const Vector4& b) noexcept {
        return length_sq(a) < length_sq(b) ? a : b;
    }

    [[nodiscard]] Vector4 min_length(const Vector4& a, const float b) noexcept {
        const float len_sq = length_sq(a);
        
        if (len_sq > b * b) {
            return a * (b / std::sqrt(len_sq));
        } else {
            return a;
        }
    }

    [[nodiscard]] Vector4 min_length(const float a, const Vector4& b) noexcept {
        return min_length(b, a);
    }

    [[nodiscard]] constexpr Vector4 max(const Vector4& a, const Vector4& b) noexcept {
        return {
            std::max(a.x, b.x),
            std::max(a.y, b.y),
            std::max(a.z, b.z),
            std::max(a.w, b.w)
        };
    }

    [[nodiscard]] constexpr Vector4 max(const Vector4& a, const float b) noexcept {
        return {
            std::max(a.x, b),
            std::max(a.y, b),
            std::max(a.z, b),
            std::max(a.w, b)
        };
    }

    [[nodiscard]] constexpr Vector4 max(const float a, const Vector4& b) noexcept {
        return max(b, a);
    }

    [[nodiscard]] constexpr Vector4 max_length(const Vector4& a, const Vector4& b) noexcept {
        return length_sq(a) > length_sq(b) ? a : b;
    }

    [[nodiscard]] Vector4 max_length(const Vector4& a, const float b) noexcept {
        const float len_sq = length_sq(a);
        
        if (len_sq < b * b) {
            return a * (b / std::sqrt(len_sq));
        } else {
            return a;
        }
    }

    [[nodiscard]] Vector4 max_length(const float a, const Vector4& b) noexcept {
        return max_length(b, a);
    }

    [[nodiscard]] constexpr Vector4 clamp(const Vector4& x, const Vector4& x_min, const Vector4& x_max) noexcept {
        return max(x_min, min(x, x_max));
    }

    [[nodiscard]] constexpr Vector4 clamp(const Vector4& x, const float x_min, const float x_max) noexcept {
        return max(x_min, min(x, x_max));
    }

    [[nodiscard]] Vector4 clamp_length(const Vector4& v, const float x_min, const float x_max) noexcept {
        const float len_sq = length_sq(v);
        
        if (len_sq < x_min * x_min) {
            return v * (x_min / std::sqrt(len_sq));
        } else if (len_sq > x_max * x_max) {
            return v * (x_max / std::sqrt(len_sq));
        } else {
            return v;
        }
    }

    [[nodiscard]] constexpr Vector4 abs(const Vector4& v) noexcept {
        return {
            std::abs(v.x),
            std::abs(v.y),
            std::abs(v.z),
            std::abs(v.w)
        };
    }

    [[nodiscard]] constexpr Vector4 floor(const Vector4& v) noexcept {
        return {
            std::floor(v.x),
            std::floor(v.y),
            std::floor(v.z),
            std::floor(v.w)
        };
    }

    [[nodiscard]] constexpr Vector4 ceil(const Vector4& v) noexcept {
        return {
            std::ceil(v.x),
            std::ceil(v.y),
            std::ceil(v.z),
            std::ceil(v.w)
        };
    }

    [[nodiscard]] constexpr Vector4 round(const Vector4& v) noexcept {
        return {
            std::round(v.x),
            std::round(v.y),
            std::round(v.z),
            std::round(v.w)
        };
    }

    [[nodiscard]] constexpr Vector4 sign(const Vector4& v) noexcept {
        return {
            v.x == 0.0f ? 0.0f : v.x > 0.0f ? 1.0f : -1.0f,
            v.y == 0.0f ? 0.0f : v.y > 0.0f ? 1.0f : -1.0f,
            v.z == 0.0f ? 0.0f : v.z > 0.0f ? 1.0f : -1.0f,
            v.w == 0.0f ? 0.0f : v.w > 0.0f ? 1.0f : -1.0f
        };
    }

    [[nodiscard]] constexpr bool approx_eq(const Vector4& a, const Vector4& b) noexcept {
        return distance_sq(a, b) < CMP_EPSILON2<float>;
    }
} // namespace draco::math

namespace std {
    template<> struct formatter<draco::math::Vector4> : std::formatter<float> {
        auto format(const draco::math::Vector4& v, std::format_context& ctx) const {
            ctx.advance_to(std::format_to(ctx.out(), "{{"));

            for (int i = 0; ; ++i) {
                ctx.advance_to(std::formatter<float>::format(v[i], ctx));

                if(i == 3) {
                    return std::format_to(ctx.out(), "}}");
                } else {
                    ctx.advance_to(std::format_to(ctx.out(), ", "));
                }
            }
        }
    };
}
