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
        [[nodiscard]] constexpr explicit Vector2(float n) noexcept;
        [[nodiscard]] constexpr Vector2(float x, float y) noexcept;
        [[nodiscard]] constexpr explicit Vector2(const Vector3& xy) noexcept;
        [[nodiscard]] constexpr explicit Vector2(const Vector4& xy) noexcept;
        
        // static
        [[nodiscard]] static constexpr Vector2 x_axis(float x = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector2 y_axis(float y = 1.0f) noexcept;
        [[nodiscard]] static Vector2 polar(float angle, float radius = 1.0f) noexcept;

        // element access
        [[nodiscard]] constexpr float& operator[](int i) noexcept;
        [[nodiscard]] constexpr const float& operator[](int i) const noexcept;

        // swizzle
        [[nodiscard]] constexpr Vector2 operator[](int i0, int i1) noexcept;
        [[nodiscard]] constexpr Vector2 operator[](int i0, int i1) const noexcept;
        [[nodiscard]] constexpr Vector3 operator[](int i0, int i1, int i2) noexcept;
        [[nodiscard]] constexpr Vector3 operator[](int i0, int i1, int i2) const noexcept;
        [[nodiscard]] constexpr Vector4 operator[](int i0, int i1, int i2, int i3) noexcept;
        [[nodiscard]] constexpr Vector4 operator[](int i0, int i1, int i2, int i3) const noexcept;

        // operators
        [[nodiscard]] constexpr Vector2 operator+() const noexcept;
        [[nodiscard]] constexpr Vector2 operator-() const noexcept;
        [[nodiscard]] constexpr bool operator==(const Vector2& other) const noexcept = default;
        constexpr Vector2& operator+=(const Vector2& other) noexcept;
        constexpr Vector2& operator+=(float other) noexcept;
        constexpr Vector2& operator-=(const Vector2& other) noexcept;
        constexpr Vector2& operator-=(float other) noexcept;
        constexpr Vector2& operator*=(const Vector2& other) noexcept;
        constexpr Vector2& operator*=(float other) noexcept;
        constexpr Vector2& operator/=(const Vector2& other) noexcept;
        constexpr Vector2& operator/=(float other) noexcept;
        constexpr Vector2& operator=(float other) noexcept;
    };

    struct alignas(16) Vector3 {
        float x, y, z;

        // constructors
        [[nodiscard]] constexpr Vector3() noexcept = default;
        [[nodiscard]] constexpr explicit Vector3(float n) noexcept;
        [[nodiscard]] constexpr Vector3(float x, float y, float z) noexcept;
        [[nodiscard]] constexpr explicit Vector3(const Vector2& xy, float z = 0.0f) noexcept;
        [[nodiscard]] constexpr Vector3(float x, const Vector2& yz) noexcept;
        [[nodiscard]] constexpr explicit Vector3(const Vector4& xyz) noexcept;
        
        // static
        [[nodiscard]] static constexpr Vector3 x_axis(float x = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector3 y_axis(float y = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector3 z_axis(float z = 1.0f) noexcept;
        [[nodiscard]] static Vector3 spherical(float azimuth, float inclination, float radius = 1.0f) noexcept;
        [[nodiscard]] static Vector3 cylindrical(float angle, float radius = 1.0f, float height = 0.0f) noexcept;

        // element access
        [[nodiscard]] constexpr float& operator[](int i) noexcept;
        [[nodiscard]] constexpr const float& operator[](int i) const noexcept;

        // swizzle
        [[nodiscard]] constexpr Vector2 operator[](int i0, int i1) noexcept;
        [[nodiscard]] constexpr Vector2 operator[](int i0, int i1) const noexcept;
        [[nodiscard]] constexpr Vector3 operator[](int i0, int i1, int i2) noexcept;
        [[nodiscard]] constexpr Vector3 operator[](int i0, int i1, int i2) const noexcept;
        [[nodiscard]] constexpr Vector4 operator[](int i0, int i1, int i2, int i3) noexcept;
        [[nodiscard]] constexpr Vector4 operator[](int i0, int i1, int i2, int i3) const noexcept;

        // operators
        [[nodiscard]] constexpr Vector3 operator+() const noexcept;
        [[nodiscard]] constexpr Vector3 operator-() const noexcept;
        [[nodiscard]] constexpr bool operator==(const Vector3& other) const noexcept = default;
        constexpr Vector3& operator+=(const Vector3& other) noexcept;
        constexpr Vector3& operator+=(float other) noexcept;
        constexpr Vector3& operator-=(const Vector3& other) noexcept;
        constexpr Vector3& operator-=(float other) noexcept;
        constexpr Vector3& operator*=(const Vector3& other) noexcept;
        constexpr Vector3& operator*=(float other) noexcept;
        constexpr Vector3& operator/=(const Vector3& other) noexcept;
        constexpr Vector3& operator/=(float other) noexcept;
        constexpr Vector3& operator=(float other) noexcept;
    };

	struct alignas(16) Vector4 {
        float x, y, z, w;

        // constructors
        [[nodiscard]] constexpr Vector4() noexcept = default;
        [[nodiscard]] constexpr explicit Vector4(float n) noexcept;
        [[nodiscard]] constexpr Vector4(float x, float y, float z, float w) noexcept;
        [[nodiscard]] constexpr explicit Vector4(const Vector2& xy) noexcept;
        [[nodiscard]] constexpr Vector4(const Vector2& xy, float z, float w) noexcept;
        [[nodiscard]] constexpr Vector4(float x, const Vector2& yz, float w) noexcept;
        [[nodiscard]] constexpr Vector4(float x, float y, const Vector2& zw) noexcept;
        [[nodiscard]] constexpr Vector4(const Vector2& xy, const Vector2& zw) noexcept;
        [[nodiscard]] constexpr explicit Vector4(const Vector3& xyz, float w = 0.0f) noexcept;
        [[nodiscard]] constexpr Vector4(float x, const Vector3& yzw) noexcept;
        
        // static
        [[nodiscard]] static constexpr Vector4 x_axis(float x = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector4 y_axis(float y = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector4 z_axis(float z = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector4 w_axis(float w = 1.0f) noexcept;

        // element access
        [[nodiscard]] constexpr float& operator[](int i) noexcept;
        [[nodiscard]] constexpr const float& operator[](int i) const noexcept;

        // swizzle
        [[nodiscard]] constexpr Vector2 operator[](int i0, int i1) noexcept;
        [[nodiscard]] constexpr Vector2 operator[](int i0, int i1) const noexcept;
        [[nodiscard]] constexpr Vector3 operator[](int i0, int i1, int i2) noexcept;
        [[nodiscard]] constexpr Vector3 operator[](int i0, int i1, int i2) const noexcept;
        [[nodiscard]] constexpr Vector4 operator[](int i0, int i1, int i2, int i3) noexcept;
        [[nodiscard]] constexpr Vector4 operator[](int i0, int i1, int i2, int i3) const noexcept;

        // member operators
        [[nodiscard]] constexpr Vector4 operator+() const noexcept;
        [[nodiscard]] constexpr Vector4 operator-() const noexcept;
        [[nodiscard]] constexpr bool operator==(const Vector4& other) const noexcept = default;
        constexpr Vector4& operator+=(const Vector4& other) noexcept;
        constexpr Vector4& operator+=(float other) noexcept;
        constexpr Vector4& operator-=(const Vector4& other) noexcept;
        constexpr Vector4& operator-=(float other) noexcept;
        constexpr Vector4& operator*=(const Vector4& other) noexcept;
        constexpr Vector4& operator*=(float other) noexcept;
        constexpr Vector4& operator/=(const Vector4& other) noexcept;
        constexpr Vector4& operator/=(float other) noexcept;
        constexpr Vector4& operator=(float other) noexcept;
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