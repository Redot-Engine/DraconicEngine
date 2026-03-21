module;

#include "platform/simd.h"

#if ARCH_X64
    #include <immintrin.h>
#elif ARCH_ARM64
    #include <arm_neon.h>
#endif

export module core.math.vector4;
import core.defs;
import std;

export namespace draco::math {

    struct alignas(16) Vector4 {
        float x, y, z, w;

        // constructors.
        constexpr Vector4() noexcept = default;
        constexpr Vector4(const float x, const float y, const float z, const float w) noexcept
        : x{x}, y{y}, z{z}, w{w} { }

        // element access.
        constexpr float& operator[](const int i) noexcept {
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

        constexpr const float& operator[](const int i) const noexcept {
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

        [[nodiscard]] constexpr bool operator==(const Vector4& other) const noexcept = default;

        constexpr Vector4& operator+=(const Vector4& other) noexcept {
            x += other.x;
            y += other.y;
            z += other.z;
            w += other.w;
            return *this;
        }

        constexpr Vector4& operator-=(const Vector4& other) noexcept {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            w -= other.w;
            return *this;
        }

        constexpr Vector4& operator*=(const Vector4& other) noexcept {
            x *= other.x;
            y *= other.y;
            z *= other.z;
            w *= other.w;
            return *this;
        }

        constexpr Vector4& operator*=(const float s) noexcept {
            x *= s;
            y *= s;
            z *= s;
            w *= s;
            return *this;
        }

        constexpr Vector4& operator/=(const float s) noexcept {
            const float inv = 1.0f / s;
            x *= inv;
            y *= inv;
            z *= inv;
            w *= inv;
            return *this;
        }
    };

    // safety features go brr
    static_assert(sizeof(Vector4) == 16, "Vector4 must be 16 bytes");
    static_assert(alignof(Vector4) == 16, "Vector4 must be 16-byte aligned");
    static_assert(trivial<Vector4>, "Vector4 must be trivial");
    static_assert(std::is_standard_layout_v<Vector4>, "Vector4 must be standard layout");

    [[nodiscard]] constexpr Vector4 operator+(const Vector4& a, const Vector4& b) noexcept {
        return { a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w };
    }

    [[nodiscard]] constexpr Vector4 operator-(const Vector4& a, const Vector4& b) noexcept {
        return { a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w };
    }

    [[nodiscard]] constexpr Vector4 operator*(const Vector4& a, const Vector4& b) noexcept {
        return { a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w };
    }

    [[nodiscard]] constexpr Vector4 operator*(const Vector4& a, const float b) noexcept {
        return { a.x*b, a.y*b, a.z*b, a.w*b };
    }

    [[nodiscard]] constexpr Vector4 operator*(const float s, const Vector4& v) noexcept {
        return v*s;
    }

    [[nodiscard]] constexpr Vector4 operator/(const Vector4& v, const float s) noexcept {
        return v * (1.f/s);
    }

    [[nodiscard]] FORCEINLINE float dot(const Vector4 &a, const Vector4 &b) noexcept {
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
} // namespace draco::math
