export module core.math.types:common;
import core.defs;

export namespace draco::math {
	struct Vector2;
	struct Vector3;
	struct Vector4;

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
}

template<typename T> consteval T select(const int i, const T v1, const T v2) {
    switch (i) {
        case 0:  return v1;
        case 1:  return v2;
        default: throw "Index out of range";
    }
}

template<typename T> consteval T select(const int i, const T v1, const T v2, const T v3) {
    switch (i) {
        case 0:  return v1;
        case 1:  return v2;
        case 2:  return v3;
        default: throw "Index out of range";
    }
}

template<typename T> consteval T select(const int i, const T v1, const T v2, const T v3, const T v4) {
    switch (i) {
        case 0:  return v1;
        case 1:  return v2;
        case 2:  return v3;
        case 3:  return v4;
        default: throw "Index out of range";
    }
}