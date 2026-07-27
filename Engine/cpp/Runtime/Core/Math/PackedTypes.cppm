module;

#include <cmath>
#include <concepts>
#include <format>
#include <limits>

export module core.math.types:packed;

import core.defs;
import core.stdtypes;
import core.math.types;
import core.math.constants;

export namespace draco::math {

    // Forward declarations
    struct Float2;
    struct Float3;
    struct Float4;
    struct Float4x4;

    /**
     * @brief Float2 is a packed 2-component vector of 32-bit floating-point values.
     */
    struct alignas(8) Float2
    {
        f32 x{0.0f}, y{0.0f};

        [[nodiscard]] constexpr Float2() noexcept = default;
        [[nodiscard]] constexpr explicit Float2(f32 n) noexcept : x{n}, y{n} {}
        [[nodiscard]] constexpr Float2(f32 x, f32 y) noexcept : x{x}, y{y} {}
        [[nodiscard]] constexpr explicit Float2(const Float3 &xyz) noexcept;
        [[nodiscard]] constexpr explicit Float2(const Float4 &xyzw) noexcept;

        [[nodiscard]] static constexpr Float2 xAxis(f32 x = 1.0f) noexcept { return {x, 0.0f}; }
        [[nodiscard]] static constexpr Float2 yAxis(f32 y = 1.0f) noexcept { return {0.0f, y}; }
        [[nodiscard]] static Float2 polar(f32 angle, f32 radius = 1.0f) noexcept
        {
            return {radius * std::cos(angle), radius * std::sin(angle)};
        }

        [[nodiscard]] constexpr f32 &operator[](i32 i) noexcept
        {
            return select(i, x, y);
        }
        [[nodiscard]] constexpr const f32 &operator[](i32 i) const noexcept
        {
            return select(i, x, y);
        }

        [[nodiscard]] constexpr Float2 operator[](i32 i0, i32 i1) const noexcept
        {
            return {select(i0, x, y), select(i1, x, y)};
        }
        [[nodiscard]] constexpr Float3 operator[](i32 i0, i32 i1, i32 i2) const noexcept;
        [[nodiscard]] constexpr Float4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) const noexcept;

        [[nodiscard]] constexpr Float2 operator+() const noexcept { return {x, y}; }
        [[nodiscard]] constexpr Float2 operator-() const noexcept { return {-x, -y}; }
        [[nodiscard]] constexpr bool operator==(const Float2 &other) const noexcept = default;

        constexpr Float2 &operator+=(const Float2 &other) noexcept
        {
            x += other.x;
            y += other.y;
            return *this;
        }
        constexpr Float2 &operator+=(f32 other) noexcept
        {
            x += other;
            y += other;
            return *this;
        }
        constexpr Float2 &operator-=(const Float2 &other) noexcept
        {
            x -= other.x;
            y -= other.y;
            return *this;
        }
        constexpr Float2 &operator-=(f32 other) noexcept
        {
            x -= other;
            y -= other;
            return *this;
        }
        constexpr Float2 &operator*=(const Float2 &other) noexcept
        {
            x *= other.x;
            y *= other.y;
            return *this;
        }
        constexpr Float2 &operator*=(f32 other) noexcept
        {
            x *= other;
            y *= other;
            return *this;
        }
        constexpr Float2 &operator/=(const Float2 &other) noexcept
        {
            x /= other.x;
            y /= other.y;
            return *this;
        }
        constexpr Float2 &operator/=(f32 other) noexcept
        {
            f32 inv = 1.0f / other;
            x *= inv;
            y *= inv;
            return *this;
        }
        constexpr Float2 &operator=(f32 other) noexcept
        {
            x = other;
            y = other;
            return *this;
        }
    };

    static_assert(sizeof(Float2) == 8 && alignof(Float2) == 8, "Float2 Layout Mismatch");

    /**
     * @brief Float3 is a packed 3-component vector of 32-bit floating-point values.
     */
    struct alignas(16) Float3
    {
        f32 x{0.0f}, y{0.0f}, z{0.0f};

        [[nodiscard]] constexpr Float3() noexcept = default;
        [[nodiscard]] constexpr explicit Float3(f32 n) noexcept : x{n}, y{n}, z{n} {}
        [[nodiscard]] constexpr Float3(f32 x, f32 y, f32 z) noexcept : x{x}, y{y}, z{z} {}
        [[nodiscard]] constexpr explicit Float3(const Float2 &xy, f32 z = 0.0f) noexcept : x{xy.x}, y{xy.y}, z{z} {}
        [[nodiscard]] constexpr Float3(f32 x, const Float2 &yz) noexcept : x{x}, y{yz.x}, z{yz.y} {}
        [[nodiscard]] constexpr explicit Float3(const Float4 &xyzw) noexcept;

        [[nodiscard]] static constexpr Float3 xAxis(f32 x = 1.0f) noexcept { return {x, 0.0f, 0.0f}; }
        [[nodiscard]] static constexpr Float3 yAxis(f32 y = 1.0f) noexcept { return {0.0f, y, 0.0f}; }
        [[nodiscard]] static constexpr Float3 zAxis(f32 z = 1.0f) noexcept { return {0.0f, 0.0f, z}; }
        [[nodiscard]] static Float3 spherical(f32 azimuth, f32 inclination, f32 radius = 1.0f) noexcept
        {
            f32 sin_incl = radius * std::sin(inclination);
            return {sin_incl * std::cos(azimuth), radius * std::cos(inclination), sin_incl * std::sin(azimuth)};
        }

        [[nodiscard]] constexpr f32 &operator[](i32 i) noexcept
        {
            return select(i, x, y, z);
        }
        [[nodiscard]] constexpr const f32 &operator[](i32 i) const noexcept
        {
            return select(i, x, y, z);
        }

        [[nodiscard]] constexpr Float2 operator[](i32 i0, i32 i1) const noexcept
        {
            return {select(i0, x, y, z), select(i1, x, y, z)};
        }
        [[nodiscard]] constexpr Float3 operator[](i32 i0, i32 i1, i32 i2) const noexcept
        {
            return {select(i0, x, y, z), select(i1, x, y, z), select(i2, x, y, z)};
        }
        [[nodiscard]] constexpr Float4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) const noexcept;

        [[nodiscard]] constexpr Float3 operator+() const noexcept { return {x, y, z}; }
        [[nodiscard]] constexpr Float3 operator-() const noexcept { return {-x, -y, -z}; }
        [[nodiscard]] constexpr bool operator==(const Float3 &other) const noexcept = default;

        constexpr Float3 &operator+=(const Float3 &other) noexcept
        {
            x += other.x;
            y += other.y;
            z += other.z;
            return *this;
        }
        constexpr Float3 &operator+=(f32 other) noexcept
        {
            x += other;
            y += other;
            z += other;
            return *this;
        }
        constexpr Float3 &operator-=(const Float3 &other) noexcept
        {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            return *this;
        }
        constexpr Float3 &operator-=(f32 other) noexcept
        {
            x -= other;
            y -= other;
            z -= other;
            return *this;
        }
        constexpr Float3 &operator*=(const Float3 &other) noexcept
        {
            x *= other.x;
            y *= other.y;
            z *= other.z;
            return *this;
        }
        constexpr Float3 &operator*=(f32 other) noexcept
        {
            x *= other;
            y *= other;
            z *= other;
            return *this;
        }
        constexpr Float3 &operator/=(const Float3 &other) noexcept
        {
            x /= other.x;
            y /= other.y;
            z /= other.z;
            return *this;
        }
        constexpr Float3 &operator/=(f32 other) noexcept
        {
            f32 inv = 1.0f / other;
            x *= inv;
            y *= inv;
            z *= inv;
            return *this;
        }
        constexpr Float3 &operator=(f32 other) noexcept
        {
            x = other;
            y = other;
            z = other;
            return *this;
        }
    };

    static_assert(sizeof(Float3) == 16 && alignof(Float3) == 16, "Float3 Layout Mismatch");

    /**
     * @brief Float4 is a packed 4-component vector of 32-bit floating-point values.
     */
    struct alignas(16) Float4
    {
        f32 x{0.0f}, y{0.0f}, z{0.0f}, w{0.0f};

        [[nodiscard]] constexpr Float4() noexcept = default;
        [[nodiscard]] constexpr explicit Float4(f32 n) noexcept : x{n}, y{n}, z{n}, w{n} {}
        [[nodiscard]] constexpr Float4(f32 x, f32 y, f32 z, f32 w) noexcept : x{x}, y{y}, z{z}, w{w} {}
        [[nodiscard]] constexpr explicit Float4(const Float2 &xy) noexcept : x{xy.x}, y{xy.y}, z{0.0f}, w{0.0f} {}
        [[nodiscard]] constexpr Float4(const Float2 &xy, f32 z, f32 w) noexcept : x{xy.x}, y{xy.y}, z{z}, w{w} {}
        [[nodiscard]] constexpr explicit Float4(const Float3 &xyz, f32 w = 0.0f) noexcept : x{xyz.x}, y{xyz.y}, z{xyz.z}, w{w} {}

        [[nodiscard]] static constexpr Float4 xAxis(f32 x = 1.0f) noexcept { return {x, 0.0f, 0.0f, 0.0f}; }
        [[nodiscard]] static constexpr Float4 yAxis(f32 y = 1.0f) noexcept { return {0.0f, y, 0.0f, 0.0f}; }
        [[nodiscard]] static constexpr Float4 zAxis(f32 z = 1.0f) noexcept { return {0.0f, 0.0f, z, 0.0f}; }
        [[nodiscard]] static constexpr Float4 wAxis(f32 w = 1.0f) noexcept { return {0.0f, 0.0f, 0.0f, w}; }

        [[nodiscard]] constexpr f32 &operator[](i32 i) noexcept
        {
            return select(i, x, y, z, w);
        }
        [[nodiscard]] constexpr const f32 &operator[](i32 i) const noexcept
        {
            return select(i, x, y, z, w);
        }

        [[nodiscard]] constexpr Float2 operator[](i32 i0, i32 i1) const noexcept
        {
            return {select(i0, x, y, z, w), select(i1, x, y, z, w)};
        }
        [[nodiscard]] constexpr Float3 operator[](i32 i0, i32 i1, i32 i2) const noexcept
        {
            return {select(i0, x, y, z, w), select(i1, x, y, z, w), select(i2, x, y, z, w)};
        }
        [[nodiscard]] constexpr Float4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) const noexcept
        {
            return {select(i0, x, y, z, w), select(i1, x, y, z, w), select(i2, x, y, z, w), select(i3, x, y, z, w)};
        }

        [[nodiscard]] constexpr Float4 operator+() const noexcept { return {x, y, z, w}; }
        [[nodiscard]] constexpr Float4 operator-() const noexcept { return {-x, -y, -z, -w}; }
        [[nodiscard]] constexpr bool operator==(const Float4 &other) const noexcept = default;

        constexpr Float4 &operator+=(const Float4 &other) noexcept
        {
            x += other.x;
            y += other.y;
            z += other.z;
            w += other.w;
            return *this;
        }
        constexpr Float4 &operator+=(f32 other) noexcept
        {
            x += other;
            y += other;
            z += other;
            w += other;
            return *this;
        }
        constexpr Float4 &operator-=(const Float4 &other) noexcept
        {
            x -= other.x;
            y -= other.y;
            z -= other.z;
            w -= other.w;
            return *this;
        }
        constexpr Float4 &operator-=(f32 other) noexcept
        {
            x -= other;
            y -= other;
            z -= other;
            w -= other;
            return *this;
        }
        constexpr Float4 &operator*=(const Float4 &other) noexcept
        {
            x *= other.x;
            y *= other.y;
            z *= other.z;
            w *= other.w;
            return *this;
        }
        constexpr Float4 &operator*=(f32 other) noexcept
        {
            x *= other;
            y *= other;
            z *= other;
            w *= other;
            return *this;
        }
        constexpr Float4 &operator/=(const Float4 &other) noexcept
        {
            x /= other.x;
            y /= other.y;
            z /= other.z;
            w /= other.w;
            return *this;
        }
        constexpr Float4 &operator/=(f32 other) noexcept
        {
            f32 inv = 1.0f / other;
            x *= inv;
            y *= inv;
            z *= inv;
            w *= inv;
            return *this;
        }
        constexpr Float4 &operator=(f32 other) noexcept
        {
            x = other;
            y = other;
            z = other;
            w = other;
            return *this;
        }
    };

    static_assert(sizeof(Float4) == 16 && alignof(Float4) == 16, "Float4 Layout Mismatch");

    // These are out of order implementations due to structural dependency
    constexpr Float2::Float2(const Float3 &xyz) noexcept : x{xyz.x}, y{xyz.y} {}
    constexpr Float2::Float2(const Float4 &xyzw) noexcept : x{xyzw.x}, y{xyzw.y} {}
    constexpr Float3::Float3(const Float4 &xyzw) noexcept : x{xyzw.x}, y{xyzw.y}, z{xyzw.z} {}
    constexpr Float3 Float2::operator[](i32 i0, i32 i1, i32 i2) const noexcept { return {select(i0, x, y), select(i1, x, y), select(i2, x, y)}; }
    constexpr Float4 Float2::operator[](i32 i0, i32 i1, i32 i2, i32 i3) const noexcept { return {select(i0, x, y), select(i1, x, y), select(i2, x, y), select(i3, x, y)}; }
    constexpr Float4 Float3::operator[](i32 i0, i32 i1, i32 i2, i32 i3) const noexcept { return {select(i0, x, y, z), select(i1, x, y, z), select(i2, x, y, z), select(i3, x, y, z)}; }

    /**
     * @brief Float4x4 is a packed 4x4 column-major matrix composed of four Float4 columns.
     */
    struct alignas(16) Float4x4
    {
        Float4 columns[4];

        [[nodiscard]] constexpr Float4x4() noexcept : columns{Float4{1.0f, 0.0f, 0.0f, 0.0f}, Float4{0.0f, 1.0f, 0.0f, 0.0f}, Float4{0.0f, 0.0f, 1.0f, 0.0f}, Float4{0.0f, 0.0f, 0.0f, 1.0f}} {}

        [[nodiscard]] constexpr explicit Float4x4(f32 diagonal) noexcept : columns{Float4{diagonal, 0.0f, 0.0f, 0.0f}, Float4{0.0f, diagonal, 0.0f, 0.0f}, Float4{0.0f, 0.0f, diagonal, 0.0f}, Float4{0.0f, 0.0f, 0.0f, diagonal}} {}

        [[nodiscard]] constexpr Float4x4(const Float4 &c0, const Float4 &c1, const Float4 &c2, const Float4 &c3) noexcept : columns{c0, c1, c2, c3} {}

        [[nodiscard]] constexpr Float4 &operator[](i32 column_idx) noexcept { return columns[column_idx]; }
        [[nodiscard]] constexpr const Float4 &operator[](i32 column_idx) const noexcept { return columns[column_idx]; }

        [[nodiscard]] constexpr bool operator==(const Float4x4 &other) const noexcept = default;

        /**
         * @brief Multiplies this matrix with another Float4x4 column-major matrix.
         */
        [[nodiscard]] constexpr Float4x4 operator*(const Float4x4 &other) const noexcept
        {
            Float4x4 result;
            for (i32 col = 0; col < 4; ++col)
            {
                for (i32 row = 0; row < 4; ++row)
                {
                    result[col][row] = columns[0][row] * other[col][0] + columns[1][row] * other[col][1] + columns[2][row] * other[col][2] + columns[3][row] * other[col][3];
                }
            }
            return result;
        }

        /**
         * @brief Multiplies this matrix by a Float4 vector.
         */
        [[nodiscard]] constexpr Float4 operator*(const Float4 &vector) const noexcept
        {
            return {
                columns[0].x * vector.x + columns[1].x * vector.y + columns[2].x * vector.z + columns[3].x * vector.w,
                columns[0].y * vector.x + columns[1].y * vector.y + columns[2].y * vector.z + columns[3].y * vector.w,
                columns[0].z * vector.x + columns[1].z * vector.y + columns[2].z * vector.z + columns[3].z * vector.w,
                columns[0].w * vector.x + columns[1].w * vector.y + columns[2].w * vector.z + columns[3].w * vector.w
            };
        }
    };

    static_assert(sizeof(Float4x4) == 64 && alignof(Float4x4) == 16, "Float4x4 Layout Mismatch");

    // Float2 Operators
    [[nodiscard]] constexpr Float2 operator+(const Float2 &a, const Float2 &b) noexcept { return {a.x + b.x, a.y + b.y}; }
    [[nodiscard]] constexpr Float2 operator+(const Float2 &a, f32 b) noexcept { return {a.x + b, a.y + b}; }
    [[nodiscard]] constexpr Float2 operator+(f32 a, const Float2 &b) noexcept { return b + a; }
    [[nodiscard]] constexpr Float2 operator-(const Float2 &a, const Float2 &b) noexcept { return {a.x - b.x, a.y - b.y}; }
    [[nodiscard]] constexpr Float2 operator-(const Float2 &a, f32 b) noexcept { return {a.x - b, a.y - b}; }
    [[nodiscard]] constexpr Float2 operator-(f32 a, const Float2 &b) noexcept { return {a - b.x, a - b.y}; }
    [[nodiscard]] constexpr Float2 operator*(const Float2 &a, const Float2 &b) noexcept { return {a.x * b.x, a.y * b.y}; }
    [[nodiscard]] constexpr Float2 operator*(const Float2 &a, f32 b) noexcept { return {a.x * b, a.y * b}; }
    [[nodiscard]] constexpr Float2 operator*(f32 a, const Float2 &b) noexcept { return b * a; }
    [[nodiscard]] constexpr Float2 operator/(const Float2 &a, const Float2 &b) noexcept { return {a.x / b.x, a.y / b.y}; }
    [[nodiscard]] constexpr Float2 operator/(const Float2 &a, f32 b) noexcept { return a * (1.0f / b); }

    // Float3 Operators
    [[nodiscard]] constexpr Float3 operator+(const Float3 &a, const Float3 &b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    [[nodiscard]] constexpr Float3 operator+(const Float3 &a, f32 b) noexcept { return {a.x + b, a.y + b, a.z + b}; }
    [[nodiscard]] constexpr Float3 operator+(f32 a, const Float3 &b) noexcept { return b + a; }
    [[nodiscard]] constexpr Float3 operator-(const Float3 &a, const Float3 &b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    [[nodiscard]] constexpr Float3 operator-(const Float3 &a, f32 b) noexcept { return {a.x - b, a.y - b, a.z - b}; }
    [[nodiscard]] constexpr Float3 operator-(f32 a, const Float3 &b) noexcept { return {a - b.x, a - b.y, a - b.z}; }
    [[nodiscard]] constexpr Float3 operator*(const Float3 &a, const Float3 &b) noexcept { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
    [[nodiscard]] constexpr Float3 operator*(const Float3 &a, f32 b) noexcept { return {a.x * b, a.y * b, a.z * b}; }
    [[nodiscard]] constexpr Float3 operator*(f32 a, const Float3 &b) noexcept { return b * a; }
    [[nodiscard]] constexpr Float3 operator/(const Float3 &a, const Float3 &b) noexcept { return {a.x / b.x, a.y / b.y, a.z / b.z}; }
    [[nodiscard]] constexpr Float3 operator/(const Float3 &a, f32 b) noexcept { return a * (1.0f / b); }

    // Float4 Operators
    [[nodiscard]] constexpr Float4 operator+(const Float4 &a, const Float4 &b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
    [[nodiscard]] constexpr Float4 operator+(const Float4 &a, f32 b) noexcept { return {a.x + b, a.y + b, a.z + b, a.w + b}; }
    [[nodiscard]] constexpr Float4 operator+(f32 a, const Float4 &b) noexcept { return b + a; }
    [[nodiscard]] constexpr Float4 operator-(const Float4 &a, const Float4 &b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
    [[nodiscard]] constexpr Float4 operator-(const Float4 &a, f32 b) noexcept { return {a.x - b, a.y - b, a.z - b, a.w - b}; }
    [[nodiscard]] constexpr Float4 operator-(f32 a, const Float4 &b) noexcept { return {a - b.x, a - b.y, a - b.z, a - b.w}; }
    [[nodiscard]] constexpr Float4 operator*(const Float4 &a, const Float4 &b) noexcept { return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; }
    [[nodiscard]] constexpr Float4 operator*(const Float4 &a, f32 b) noexcept { return {a.x * b, a.y * b, a.z * b, a.w * b}; }
    [[nodiscard]] constexpr Float4 operator*(f32 a, const Float4 &b) noexcept { return b * a; }
    [[nodiscard]] constexpr Float4 operator/(const Float4 &a, const Float4 &b) noexcept { return {a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w}; }
    [[nodiscard]] constexpr Float4 operator/(const Float4 &a, f32 b) noexcept { return a * (1.0f / b); }

    // Vector Operations
    [[nodiscard]] constexpr f32 dot(const Float2 &a, const Float2 &b) noexcept { return a.x * b.x + a.y * b.y; }
    [[nodiscard]] constexpr f32 dot(const Float3 &a, const Float3 &b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
    [[nodiscard]] constexpr f32 dot(const Float4 &a, const Float4 &b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

    [[nodiscard]] constexpr f32 length_sq(const Float2 &v) noexcept { return dot(v, v); }
    [[nodiscard]] constexpr f32 length_sq(const Float3 &v) noexcept { return dot(v, v); }
    [[nodiscard]] constexpr f32 length_sq(const Float4 &v) noexcept { return dot(v, v); }

    [[nodiscard]] f32 length(const Float2 &v) noexcept { return std::sqrt(length_sq(v)); }
    [[nodiscard]] f32 length(const Float3 &v) noexcept { return std::sqrt(length_sq(v)); }
    [[nodiscard]] f32 length(const Float4 &v) noexcept { return std::sqrt(length_sq(v)); }

    [[nodiscard]] Float2 normalize(const Float2 &v) noexcept
    {
        f32 len = length(v);
        return (len > CMP_NORMALIZE_TOLERANCE) ? v / len : Float2();
    }
    [[nodiscard]] Float3 normalize(const Float3 &v) noexcept
    {
        f32 len = length(v);
        return (len > CMP_NORMALIZE_TOLERANCE) ? v / len : Float3();
    }
    [[nodiscard]] Float4 normalize(const Float4 &v) noexcept
    {
        f32 len = length(v);
        return (len > CMP_NORMALIZE_TOLERANCE) ? v / len : Float4();
    }

    [[nodiscard]] constexpr Float3 cross(const Float3 &a, const Float3 &b) noexcept
    {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
} // namespace draco::math

// Custom STD formatters for Math types
export namespace std {
    template <>
    struct formatter<draco::math::Float2> : formatter<draco::f32>
    {
        auto format(const draco::math::Float2 &v, format_context &ctx) const
        {
            ctx.advance_to(format_to(ctx.out(), "{{"));
            ctx.advance_to(formatter<draco::f32>::format(v.x, ctx));
            ctx.advance_to(format_to(ctx.out(), ", "));
            ctx.advance_to(formatter<draco::f32>::format(v.y, ctx));
            return format_to(ctx.out(), "}}");
        }
    };

    template <>
    struct formatter<draco::math::Float3> : formatter<draco::f32>
    {
        auto format(const draco::math::Float3 &v, format_context &ctx) const
        {
            ctx.advance_to(format_to(ctx.out(), "{{"));
            for (draco::i32 i = 0; i < 3; ++i)
            {
                if (i)
                    ctx.advance_to(format_to(ctx.out(), ", "));
                ctx.advance_to(formatter<draco::f32>::format(v[i], ctx));
            }
            return format_to(ctx.out(), "}}");
        }
    };

    template <>
    struct formatter<draco::math::Float4> : formatter<draco::f32>
    {
        auto format(const draco::math::Float4 &v, format_context &ctx) const
        {
            ctx.advance_to(format_to(ctx.out(), "{{"));
            for (draco::i32 i = 0; i < 4; ++i)
            {
                if (i)
                    ctx.advance_to(format_to(ctx.out(), ", "));
                ctx.advance_to(formatter<draco::f32>::format(v[i], ctx));
            }
            return format_to(ctx.out(), "}}");
        }
    };
}
