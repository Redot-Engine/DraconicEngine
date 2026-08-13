export module core.math.types:common;
import core.defs;

export namespace draco::math {
	struct Vector2;
	struct Vector3;
	struct Vector4;

    struct alignas(8) Vector2 {
        f32 x, y;

        // constructors
        [[nodiscard]] constexpr Vector2() noexcept = default;
        [[nodiscard]] constexpr explicit Vector2(f32 n) noexcept;
        [[nodiscard]] constexpr Vector2(f32 x, f32 y) noexcept;
        [[nodiscard]] constexpr explicit Vector2(const Vector3& xy) noexcept;
        [[nodiscard]] constexpr explicit Vector2(const Vector4& xy) noexcept;
        
        // static
        [[nodiscard]] static constexpr Vector2 xAxis(f32 x = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector2 yAxis(f32 y = 1.0f) noexcept;
        [[nodiscard]] static Vector2 polar(f32 angle, f32 radius = 1.0f) noexcept;

        // element access
        [[nodiscard]] constexpr f32& operator[](i32 i) noexcept;
        [[nodiscard]] constexpr const f32& operator[](i32 i) const noexcept;

        // swizzle
        [[nodiscard]] constexpr Vector2 operator[](i32 i0, i32 i1) const noexcept;
        [[nodiscard]] constexpr Vector3 operator[](i32 i0, i32 i1, i32 i2) const noexcept;
        [[nodiscard]] constexpr Vector4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) const noexcept;

        // operators
        [[nodiscard]] constexpr Vector2 operator+() const noexcept;
        [[nodiscard]] constexpr Vector2 operator-() const noexcept;
        [[nodiscard]] constexpr bool operator==(const Vector2& other) const noexcept = default;
        constexpr Vector2& operator+=(const Vector2& other) noexcept;
        constexpr Vector2& operator+=(f32 other) noexcept;
        constexpr Vector2& operator-=(const Vector2& other) noexcept;
        constexpr Vector2& operator-=(f32 other) noexcept;
        constexpr Vector2& operator*=(const Vector2& other) noexcept;
        constexpr Vector2& operator*=(f32 other) noexcept;
        constexpr Vector2& operator/=(const Vector2& other) noexcept;
        constexpr Vector2& operator/=(f32 other) noexcept;
        constexpr Vector2& operator=(f32 other) noexcept;
    };

    struct alignas(16) Vector3 {
        f32 x, y, z;

        // constructors
        [[nodiscard]] constexpr Vector3() noexcept = default;
        [[nodiscard]] constexpr explicit Vector3(f32 n) noexcept;
        [[nodiscard]] constexpr Vector3(f32 x, f32 y, f32 z) noexcept;
        [[nodiscard]] constexpr explicit Vector3(const Vector2& xy, f32 z = 0.0f) noexcept;
        [[nodiscard]] constexpr Vector3(f32 x, const Vector2& yz) noexcept;
        [[nodiscard]] constexpr explicit Vector3(const Vector4& xyz) noexcept;
        
        // static
        [[nodiscard]] static constexpr Vector3 xAxis(f32 x = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector3 yAxis(f32 y = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector3 zAxis(f32 z = 1.0f) noexcept;
        [[nodiscard]] static Vector3 spherical(f32 azimuth, f32 inclination, f32 radius = 1.0f) noexcept;
        [[nodiscard]] static Vector3 cylindrical(f32 angle, f32 radius = 1.0f, f32 height = 0.0f) noexcept;

        // constant vectors (defined in Vector3.cppm)
        [[nodiscard]] static constexpr Vector3 zero() noexcept;
        [[nodiscard]] static constexpr Vector3 one() noexcept;

        // element access
        [[nodiscard]] constexpr f32& operator[](i32 i) noexcept;
        [[nodiscard]] constexpr const f32& operator[](i32 i) const noexcept;

        // swizzle
        [[nodiscard]] constexpr Vector2 operator[](i32 i0, i32 i1) const noexcept;
        [[nodiscard]] constexpr Vector3 operator[](i32 i0, i32 i1, i32 i2) const noexcept;
        [[nodiscard]] constexpr Vector4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) const noexcept;

        // operators
        [[nodiscard]] constexpr Vector3 operator+() const noexcept;
        [[nodiscard]] constexpr Vector3 operator-() const noexcept;
        [[nodiscard]] constexpr bool operator==(const Vector3& other) const noexcept = default;
        constexpr Vector3& operator+=(const Vector3& other) noexcept;
        constexpr Vector3& operator+=(f32 other) noexcept;
        constexpr Vector3& operator-=(const Vector3& other) noexcept;
        constexpr Vector3& operator-=(f32 other) noexcept;
        constexpr Vector3& operator*=(const Vector3& other) noexcept;
        constexpr Vector3& operator*=(f32 other) noexcept;
        constexpr Vector3& operator/=(const Vector3& other) noexcept;
        constexpr Vector3& operator/=(f32 other) noexcept;
        constexpr Vector3& operator=(f32 other) noexcept;
    };

	struct alignas(16) Vector4 {
        f32 x, y, z, w;

        // constructors
        [[nodiscard]] constexpr Vector4() noexcept = default;
        [[nodiscard]] constexpr explicit Vector4(f32 n) noexcept;
        [[nodiscard]] constexpr Vector4(f32 x, f32 y, f32 z, f32 w) noexcept;
        [[nodiscard]] constexpr explicit Vector4(const Vector2& xy) noexcept;
        [[nodiscard]] constexpr Vector4(const Vector2& xy, f32 z, f32 w) noexcept;
        [[nodiscard]] constexpr Vector4(f32 x, const Vector2& yz, f32 w) noexcept;
        [[nodiscard]] constexpr Vector4(f32 x, f32 y, const Vector2& zw) noexcept;
        [[nodiscard]] constexpr Vector4(const Vector2& xy, const Vector2& zw) noexcept;
        [[nodiscard]] constexpr explicit Vector4(const Vector3& xyz, f32 w = 0.0f) noexcept;
        [[nodiscard]] constexpr Vector4(f32 x, const Vector3& yzw) noexcept;
        
        // static
        [[nodiscard]] static constexpr Vector4 xAxis(f32 x = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector4 yAxis(f32 y = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector4 zAxis(f32 z = 1.0f) noexcept;
        [[nodiscard]] static constexpr Vector4 wAxis(f32 w = 1.0f) noexcept;

        // element access
        [[nodiscard]] constexpr f32& operator[](i32 i) noexcept;
        [[nodiscard]] constexpr const f32& operator[](i32 i) const noexcept;

        // swizzle
        [[nodiscard]] constexpr Vector2 operator[](i32 i0, i32 i1) const noexcept;
        [[nodiscard]] constexpr Vector3 operator[](i32 i0, i32 i1, i32 i2) const noexcept;
        [[nodiscard]] constexpr Vector4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) const noexcept;

        // member operators
        [[nodiscard]] constexpr Vector4 operator+() const noexcept;
        [[nodiscard]] constexpr Vector4 operator-() const noexcept;
        [[nodiscard]] constexpr bool operator==(const Vector4& other) const noexcept = default;
        constexpr Vector4& operator+=(const Vector4& other) noexcept;
        constexpr Vector4& operator+=(f32 other) noexcept;
        constexpr Vector4& operator-=(const Vector4& other) noexcept;
        constexpr Vector4& operator-=(f32 other) noexcept;
        constexpr Vector4& operator*=(const Vector4& other) noexcept;
        constexpr Vector4& operator*=(f32 other) noexcept;
        constexpr Vector4& operator/=(const Vector4& other) noexcept;
        constexpr Vector4& operator/=(f32 other) noexcept;
        constexpr Vector4& operator=(f32 other) noexcept;
    };
}

template<typename T>
consteval const T& select(const draco::i32 i, const T& v1, const T& v2) {
    switch (i) {
        case 0:  return v1;
        case 1:  return v2;
        default: throw "Index out of range";
    }
}

template<typename T>
consteval T& select(const draco::i32 i, T& v1, T& v2) {
    switch (i) {
        case 0:  return v1;
        case 1:  return v2;
        default: throw "Index out of range";
    }
}

template<typename T>
consteval const T& select(const draco::i32 i, const T& v1, const T& v2, const T& v3) {
    switch (i) {
        case 0:  return v1;
        case 1:  return v2;
        case 2:  return v3;
        default: throw "Index out of range";
    }
}

template<typename T>
consteval T& select(const draco::i32 i, T& v1, T& v2, T& v3) {
    switch (i) {
        case 0:  return v1;
        case 1:  return v2;
        case 2:  return v3;
        default: throw "Index out of range";
    }
}

template<typename T>
consteval const T& select(const draco::i32 i, const T& v1, const T& v2, const T& v3, const T& v4) {
    switch (i) {
        case 0:  return v1;
        case 1:  return v2;
        case 2:  return v3;
        case 3:  return v4;
        default: throw "Index out of range";
    }
}

template<typename T>
consteval T& select(const draco::i32 i, T& v1, T& v2, T& v3, T& v4) {
    switch (i) {
        case 0:  return v1;
        case 1:  return v2;
        case 2:  return v3;
        case 3:  return v4;
        default: throw "Index out of range";
    }
}