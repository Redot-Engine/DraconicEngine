module;

#include "platform/simd.h"

export module core.math.types;
export import std;
import core.defs;

template<typename T> consteval T select(const int i, const T v1, const T v2, const T v3) {
    switch (i) {
        case 0: return v1;
        case 1: return v2;
        default:
        case 2: return v3;
    }
}

template<typename T> consteval T select(const int i, const T v1, const T v2, const T v3, const T v4) {
    switch (i) {
        case 0: return v1;
        case 1: return v2;
        case 2: return v3;
        default:
        case 3: return v4;
    }
}

export namespace draco::math {
	struct Vector2;
	struct Vector3;
	struct Vector4;

    // ===================== //
    //        Vector2        //
    // ===================== //

    struct alignas(8) Vector2 {
        float x, y;

        // constructors
        [[nodiscard]] constexpr Vector2() noexcept = default;
        [[nodiscard]] constexpr Vector2(const float n) noexcept;
        [[nodiscard]] constexpr Vector2(const float x, const float y) noexcept;
        [[nodiscard]] constexpr Vector2(const Vector3& xy) noexcept;
        [[nodiscard]] constexpr Vector2(const Vector4& xy) noexcept;
        
        // static
        [[nodiscard]] static constexpr Vector2 x_axis(const float x = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector2 y_axis(const float y = 1.0f) noexcept;
        [[nodiscard]] static Vector2 polar(const float angle, const float radius = 1.0f) noexcept;

        // element access
        [[nodiscard]] constexpr float& operator[](const int i) noexcept;
        [[nodiscard]] constexpr const float& operator[](const int i) const noexcept;

        // swizzle
        [[nodiscard]] constexpr Vector2 operator[](const int i0, const int i1) noexcept;
        [[nodiscard]] constexpr Vector2 operator[](const int i0, const int i1) const noexcept;
        [[nodiscard]] constexpr Vector3 operator[](const int i0, const int i1, const int i2) noexcept;
        [[nodiscard]] constexpr Vector3 operator[](const int i0, const int i1, const int i2) const noexcept;
        [[nodiscard]] constexpr Vector4 operator[](const int i0, const int i1, const int i2, const int i3) noexcept;
        [[nodiscard]] constexpr Vector4 operator[](const int i0, const int i1, const int i2, const int i3) const noexcept;

        // operators
        [[nodiscard]] constexpr Vector2 operator+() noexcept;
        [[nodiscard]] constexpr Vector2 operator-() noexcept;
        [[nodiscard]] constexpr bool operator==(const Vector2& other) const noexcept = default;
        constexpr Vector2& operator+=(const Vector2& other) noexcept;
        constexpr Vector2& operator+=(const float other) noexcept;
        constexpr Vector2& operator-=(const Vector2& other) noexcept;
        constexpr Vector2& operator-=(const float other) noexcept;
        constexpr Vector2& operator*=(const Vector2& other) noexcept;
        constexpr Vector2& operator*=(const float other) noexcept;
        constexpr Vector2& operator/=(const Vector2& other) noexcept;
        constexpr Vector2& operator/=(const float other) noexcept;
        constexpr Vector2& operator=(const float other) noexcept;
    };
    
    // non-member operators
    [[nodiscard]] constexpr Vector2 operator+(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector2 operator+(const Vector2& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector2 operator+(const float a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector2 operator-(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector2 operator-(const Vector2& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector2 operator-(const float a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector2 operator*(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector2 operator*(const Vector2& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector2 operator*(const float a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector2 operator/(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector2 operator/(const Vector2& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector2 operator/(const float a, const Vector2& b) noexcept;

    // assertions
    static_assert(sizeof(Vector2) == 8, "Vector2 must be 8 bytes");
    static_assert(alignof(Vector2) == 8, "Vector2 must be 8-byte aligned");
    static_assert(trivial<Vector2>, "Vector2 must be trivial");
    static_assert(std::is_standard_layout_v<Vector2>, "Vector2 must be standard layout");

    // ===================== //
    //        Vector3        //
    // ===================== //

    struct alignas(16) Vector3 {
        float x, y, z;

        // constructors
        [[nodiscard]] constexpr Vector3() noexcept = default;
        [[nodiscard]] constexpr Vector3(const float n) noexcept;
        [[nodiscard]] constexpr Vector3(const float x, const float y, const float z) noexcept;
        [[nodiscard]] constexpr Vector3(const Vector2& xy, const float z = 0.0f) noexcept;
        [[nodiscard]] constexpr Vector3(const float x, const Vector2& yz) noexcept;
        [[nodiscard]] constexpr Vector3(const Vector4& xyz) noexcept;
        
        // static
        [[nodiscard]] static constexpr Vector3 x_axis(const float x = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector3 y_axis(const float y = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector3 z_axis(const float z = 1.0f) noexcept;
        [[nodiscard]] static Vector3 spherical(const float azimuth, const float inclination, const float radius = 1.0f) noexcept;
        [[nodiscard]] static Vector3 cylindrical(const float angle, const float radius = 1.0f, const float height = 0.0f) noexcept;

        // element access
        [[nodiscard]] constexpr float& operator[](const int i) noexcept;
        [[nodiscard]] constexpr const float& operator[](const int i) const noexcept;

        // swizzle
        [[nodiscard]] constexpr Vector2 operator[](const int i0, const int i1) noexcept;
        [[nodiscard]] constexpr Vector2 operator[](const int i0, const int i1) const noexcept;
        [[nodiscard]] constexpr Vector3 operator[](const int i0, const int i1, const int i2) noexcept;
        [[nodiscard]] constexpr Vector3 operator[](const int i0, const int i1, const int i2) const noexcept;
        [[nodiscard]] constexpr Vector4 operator[](const int i0, const int i1, const int i2, const int i3) noexcept;
        [[nodiscard]] constexpr Vector4 operator[](const int i0, const int i1, const int i2, const int i3) const noexcept;

        // operators
        [[nodiscard]] constexpr Vector3 operator+() noexcept;
        [[nodiscard]] constexpr Vector3 operator-() noexcept;
        [[nodiscard]] constexpr bool operator==(const Vector3& other) const noexcept = default;
        constexpr Vector3& operator+=(const Vector3& other) noexcept;
        constexpr Vector3& operator+=(const float other) noexcept;
        constexpr Vector3& operator-=(const Vector3& other) noexcept;
        constexpr Vector3& operator-=(const float other) noexcept;
        constexpr Vector3& operator*=(const Vector3& other) noexcept;
        constexpr Vector3& operator*=(const float other) noexcept;
        constexpr Vector3& operator/=(const Vector3& other) noexcept;
        constexpr Vector3& operator/=(const float other) noexcept;
        constexpr Vector3& operator=(const float other) noexcept;
    };
    
    // non-member operators
    [[nodiscard]] constexpr Vector3 operator+(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector3 operator+(const Vector3& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector3 operator+(const float a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector3 operator-(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector3 operator-(const Vector3& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector3 operator-(const float a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector3 operator*(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector3 operator*(const Vector3& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector3 operator*(const float a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector3 operator/(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector3 operator/(const Vector3& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector3 operator/(const float a, const Vector3& b) noexcept;

    // assertions
    static_assert(sizeof(Vector3) == 16, "Vector3 must be 16 bytes");
    static_assert(alignof(Vector3) == 16, "Vector3 must be 16-byte aligned");
    static_assert(trivial<Vector3>, "Vector3 must be trivial");
    static_assert(std::is_standard_layout_v<Vector3>, "Vector3 must be standard layout");

    // ===================== //
    //        Vector4        //
    // ===================== //

	struct alignas(16) Vector4 {
        float x, y, z, w;

        // constructors
        [[nodiscard]] constexpr Vector4() noexcept = default;
        [[nodiscard]] constexpr Vector4(const float n) noexcept;
        [[nodiscard]] constexpr Vector4(const float x, const float y, const float z, const float w) noexcept;
        [[nodiscard]] constexpr Vector4(const Vector2& xy) noexcept;
        [[nodiscard]] constexpr Vector4(const Vector2& xy, const float z, const float w) noexcept;
        [[nodiscard]] constexpr Vector4(const float x, const Vector2& yz, const float w) noexcept;
        [[nodiscard]] constexpr Vector4(const float x, const float y, const Vector2& zw) noexcept;
        [[nodiscard]] constexpr Vector4(const Vector2& xy, const Vector2& zw) noexcept;
        [[nodiscard]] constexpr Vector4(const Vector3& xyz, const float w = 0.0f) noexcept;
        [[nodiscard]] constexpr Vector4(const float x, const Vector3& yzw) noexcept;
        
        // static
        [[nodiscard]] static constexpr Vector4 x_axis(const float x = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector4 y_axis(const float y = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector4 z_axis(const float z = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector4 w_axis(const float w = 1.0f) noexcept;

        // element access
        [[nodiscard]] constexpr float& operator[](const int i) noexcept;
        [[nodiscard]] constexpr const float& operator[](const int i) const noexcept;

        // swizzle
        [[nodiscard]] constexpr Vector2 operator[](const int i0, const int i1) noexcept;
        [[nodiscard]] constexpr Vector2 operator[](const int i0, const int i1) const noexcept;
        [[nodiscard]] constexpr Vector3 operator[](const int i0, const int i1, const int i2) noexcept;
        [[nodiscard]] constexpr Vector3 operator[](const int i0, const int i1, const int i2) const noexcept;
        [[nodiscard]] constexpr Vector4 operator[](const int i0, const int i1, const int i2, const int i3) noexcept;
        [[nodiscard]] constexpr Vector4 operator[](const int i0, const int i1, const int i2, const int i3) const noexcept;

        // member operators
        [[nodiscard]] constexpr Vector4 operator+() noexcept;
        [[nodiscard]] constexpr Vector4 operator-() noexcept;
        [[nodiscard]] constexpr bool operator==(const Vector4& other) const noexcept = default;
        constexpr Vector4& operator+=(const Vector4& other) noexcept;
        constexpr Vector4& operator+=(const float other) noexcept;
        constexpr Vector4& operator-=(const Vector4& other) noexcept;
        constexpr Vector4& operator-=(const float other) noexcept;
        constexpr Vector4& operator*=(const Vector4& other) noexcept;
        constexpr Vector4& operator*=(const float other) noexcept;
        constexpr Vector4& operator/=(const Vector4& other) noexcept;
        constexpr Vector4& operator/=(const float other) noexcept;
        constexpr Vector4& operator=(const float other) noexcept;
    };
    
    // non-member operators
    [[nodiscard]] constexpr Vector4 operator+(const Vector4& a, const Vector4& b) noexcept;
    [[nodiscard]] constexpr Vector4 operator+(const Vector4& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector4 operator+(const float a, const Vector4& b) noexcept;
    [[nodiscard]] constexpr Vector4 operator-(const Vector4& a, const Vector4& b) noexcept;
    [[nodiscard]] constexpr Vector4 operator-(const Vector4& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector4 operator-(const float a, const Vector4& b) noexcept;
    [[nodiscard]] constexpr Vector4 operator*(const Vector4& a, const Vector4& b) noexcept;
    [[nodiscard]] constexpr Vector4 operator*(const Vector4& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector4 operator*(const float a, const Vector4& b) noexcept;
    [[nodiscard]] constexpr Vector4 operator/(const Vector4& a, const Vector4& b) noexcept;
    [[nodiscard]] constexpr Vector4 operator/(const Vector4& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector4 operator/(const float a, const Vector4& b) noexcept;

    // assertions
    static_assert(sizeof(Vector4) == 16, "Vector4 must be 16 bytes");
    static_assert(alignof(Vector4) == 16, "Vector4 must be 16-byte aligned");
    static_assert(trivial<Vector4>, "Vector4 must be trivial");
    static_assert(std::is_standard_layout_v<Vector4>, "Vector4 must be standard layout");

    // ===================== //
    //       Functions       //
    // ===================== //

    // Returns dot product
    [[nodiscard]] constexpr float dot(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr float dot(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr float dot(const Vector4& a, const Vector4& b) noexcept;

    // Returns squared magnitude
    [[nodiscard]] constexpr float length_sq(const Vector2& v) noexcept;
    [[nodiscard]] constexpr float length_sq(const Vector3& v) noexcept;
    [[nodiscard]] constexpr float length_sq(const Vector4& v) noexcept;

    // Returns magnitude
    [[nodiscard]] float length(const Vector2& v) noexcept;
    [[nodiscard]] float length(const Vector3& v) noexcept;
    [[nodiscard]] float length(const Vector4& v) noexcept;

    // Return squared distance between two vectors
    [[nodiscard]] constexpr float distance_sq(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr float distance_sq(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr float distance_sq(const Vector4& a, const Vector4& b) noexcept;

    // Returns distance between two vectors
    [[nodiscard]] float distance(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] float distance(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] float distance(const Vector4& a, const Vector4& b) noexcept;

    // Safe normalize, checks length
    [[nodiscard]] Vector2 normalize(const Vector2& v) noexcept;
    [[nodiscard]] Vector3 normalize(const Vector3& v) noexcept;
    [[nodiscard]] Vector4 normalize(const Vector4& v) noexcept;
    
    // Faster normalize, it presupposes vector has non-zero length
    [[nodiscard]] Vector2 normalize_fast(const Vector2& v) noexcept;
    [[nodiscard]] Vector3 normalize_fast(const Vector3& v) noexcept;
    [[nodiscard]] Vector4 normalize_fast(const Vector4& v) noexcept;

    // Returns vector projected onto normal
    [[nodiscard]] constexpr Vector2 project(const Vector2& vector, const Vector2& normal) noexcept;
    [[nodiscard]] constexpr Vector3 project(const Vector3& vector, const Vector3& normal) noexcept;
    [[nodiscard]] constexpr Vector4 project(const Vector4& vector, const Vector4& normal) noexcept;

    // Returns a vector reflected off a plane defined by its normal
    [[nodiscard]] constexpr Vector2 reflect(const Vector2& incoming, const Vector2& normal) noexcept;
    [[nodiscard]] constexpr Vector3 reflect(const Vector3& incoming, const Vector3& normal) noexcept;
    [[nodiscard]] constexpr Vector4 reflect(const Vector4& incoming, const Vector4& normal) noexcept;

    // Returns the angle between two vectors
    [[nodiscard]] float angle(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] float angle(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] float angle(const Vector4& a, const Vector4& b) noexcept;

    // Returns linear interpolation between two vectors
    [[nodiscard]] constexpr Vector2 lerp(const Vector2& from, const Vector2& to, const float weight) noexcept;
    [[nodiscard]] constexpr Vector3 lerp(const Vector3& from, const Vector3& to, const float weight) noexcept;
    [[nodiscard]] constexpr Vector4 lerp(const Vector4& from, const Vector4& to, const float weight) noexcept;

    // Returns component-wise minimum
    [[nodiscard]] constexpr Vector2 min(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector3 min(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector4 min(const Vector4& a, const Vector4& b) noexcept;

    [[nodiscard]] constexpr Vector2 min(const Vector2& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector3 min(const Vector3& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector4 min(const Vector4& a, const float b) noexcept;

    [[nodiscard]] constexpr Vector2 min(const float a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector3 min(const float a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector4 min(const float a, const Vector4& b) noexcept;

    // Returns the vector with the smaller length
    [[nodiscard]] constexpr Vector2 min_length(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector3 min_length(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector4 min_length(const Vector4& a, const Vector4& b) noexcept;

    // Returns a vector in the same direction whose length is bounded above by the given value  
    [[nodiscard]] Vector2 min_length(const Vector2& a, const float b) noexcept;
    [[nodiscard]] Vector3 min_length(const Vector3& a, const float b) noexcept;
    [[nodiscard]] Vector4 min_length(const Vector4& a, const float b) noexcept;

    [[nodiscard]] Vector2 min_length(const float a, const Vector2& b) noexcept;
    [[nodiscard]] Vector3 min_length(const float a, const Vector3& b) noexcept;
    [[nodiscard]] Vector4 min_length(const float a, const Vector4& b) noexcept;

    // Returns component-wise maximum
    [[nodiscard]] constexpr Vector2 max(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector3 max(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector4 max(const Vector4& a, const Vector4& b) noexcept;

    [[nodiscard]] constexpr Vector2 max(const Vector2& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector3 max(const Vector3& a, const float b) noexcept;
    [[nodiscard]] constexpr Vector4 max(const Vector4& a, const float b) noexcept;

    [[nodiscard]] constexpr Vector2 max(const float a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector3 max(const float a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector4 max(const float a, const Vector4& b) noexcept;

    // Returns the vector with the larger length
    [[nodiscard]] constexpr Vector2 max_length(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr Vector3 max_length(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr Vector4 max_length(const Vector4& a, const Vector4& b) noexcept;

    // Returns a vector in the same direction whose length is bounded below by the given value  
    [[nodiscard]] Vector2 max_length(const Vector2& a, const float b) noexcept;
    [[nodiscard]] Vector3 max_length(const Vector3& a, const float b) noexcept;
    [[nodiscard]] Vector4 max_length(const Vector4& a, const float b) noexcept;

    [[nodiscard]] Vector2 max_length(const float a, const Vector2& b) noexcept;
    [[nodiscard]] Vector3 max_length(const float a, const Vector3& b) noexcept;
    [[nodiscard]] Vector4 max_length(const float a, const Vector4& b) noexcept;

    // Clamps each component of x to the range [x_min, x_max]. Presupposes x_min <= x_max
    [[nodiscard]] constexpr Vector2 clamp(const Vector2& x, const Vector2& x_min, const Vector2& x_max) noexcept;
    [[nodiscard]] constexpr Vector3 clamp(const Vector3& x, const Vector3& x_min, const Vector3& x_max) noexcept;
    [[nodiscard]] constexpr Vector4 clamp(const Vector4& x, const Vector4& x_min, const Vector4& x_max) noexcept;

    [[nodiscard]] constexpr Vector2 clamp(const Vector2& x, const float x_min, const float x_max) noexcept;
    [[nodiscard]] constexpr Vector3 clamp(const Vector3& x, const float x_min, const float x_max) noexcept;
    [[nodiscard]] constexpr Vector4 clamp(const Vector4& x, const float x_min, const float x_max) noexcept;

    // Clamps the length of the vector to the range [x_min, x_max]. Presupposes x_min <= x_max.
    [[nodiscard]] Vector2 clamp_length(const Vector2& v, const float x_min, const float x_max) noexcept;
    [[nodiscard]] Vector3 clamp_length(const Vector3& v, const float x_min, const float x_max) noexcept;
    [[nodiscard]] Vector4 clamp_length(const Vector4& v, const float x_min, const float x_max) noexcept;

    // Returns component-wise absolute value
    [[nodiscard]] constexpr Vector2 abs(const Vector2& v) noexcept;
    [[nodiscard]] constexpr Vector3 abs(const Vector3& v) noexcept;
    [[nodiscard]] constexpr Vector4 abs(const Vector4& v) noexcept;

    // Returns component-wise floor
    [[nodiscard]] constexpr Vector2 floor(const Vector2& v) noexcept;
    [[nodiscard]] constexpr Vector3 floor(const Vector3& v) noexcept;
    [[nodiscard]] constexpr Vector4 floor(const Vector4& v) noexcept;

    // Returns component-wise ceiling
    [[nodiscard]] constexpr Vector2 ceil(const Vector2& v) noexcept;
    [[nodiscard]] constexpr Vector3 ceil(const Vector3& v) noexcept;
    [[nodiscard]] constexpr Vector4 ceil(const Vector4& v) noexcept;

    // Returns component-wise round
    [[nodiscard]] constexpr Vector2 round(const Vector2& v) noexcept;
    [[nodiscard]] constexpr Vector3 round(const Vector3& v) noexcept;
    [[nodiscard]] constexpr Vector4 round(const Vector4& v) noexcept;

    // Returns component-wise sign. Note that -0 still returns 0
    [[nodiscard]] constexpr Vector2 sign(const Vector2& v) noexcept;
    [[nodiscard]] constexpr Vector3 sign(const Vector3& v) noexcept;
    [[nodiscard]] constexpr Vector4 sign(const Vector4& v) noexcept;

    // Returns true if the vectors are approximately equal
    [[nodiscard]] constexpr bool approx_eq(const Vector2& a, const Vector2& b) noexcept;
    [[nodiscard]] constexpr bool approx_eq(const Vector3& a, const Vector3& b) noexcept;
    [[nodiscard]] constexpr bool approx_eq(const Vector4& a, const Vector4& b) noexcept;

    // Returns cross product
    [[nodiscard]] constexpr Vector3 cross(const Vector3& a, const Vector3& b) noexcept;

    // Returns a new vector rotated counter-clockwise in radians
    [[nodiscard]] constexpr Vector2 rotate(const Vector2& v, const float angle) noexcept;
}

export namespace std {
    template<> struct formatter<draco::math::Vector2> : std::formatter<float> {
        auto format(const draco::math::Vector2& v, std::format_context& ctx) const;
    };

    template<> struct formatter<draco::math::Vector3> : std::formatter<float> {
        auto format(const draco::math::Vector3& v, std::format_context& ctx) const;
    };
    
    template<> struct formatter<draco::math::Vector4> : std::formatter<float> {
        auto format(const draco::math::Vector4& v, std::format_context& ctx) const;
    };
}