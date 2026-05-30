export module core.math.types:common;
import core.defs;
import core.stdtypes;

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
	[[nodiscard]] constexpr explicit Vector2(Vector3 const &xy) noexcept;
	[[nodiscard]] constexpr explicit Vector2(Vector4 const &xy) noexcept;

	// static
	[[nodiscard]] static constexpr Vector2 x_axis(f32 x = 1.0F) noexcept;
	[[nodiscard]] static constexpr Vector2 y_axis(f32 y = 1.0F) noexcept;
	[[nodiscard]] static Vector2 polar(f32 angle, f32 radius = 1.0F) noexcept;

	// element access
	[[nodiscard]] constexpr f32 &operator[](i32 i) noexcept;
	[[nodiscard]] constexpr f32 const &operator[](i32 i) const noexcept;

	// swizzle
	[[nodiscard]] constexpr Vector2 operator[](i32 i0, i32 i1) noexcept;
	[[nodiscard]] constexpr Vector2 operator[](i32 i0, i32 i1) const noexcept;
	[[nodiscard]] constexpr Vector3 operator[](i32 i0, i32 i1, i32 i2) noexcept;
	[[nodiscard]] constexpr Vector3 operator[](i32 i0, i32 i1, i32 i2) const noexcept;
	[[nodiscard]] constexpr Vector4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) noexcept;
	[[nodiscard]] constexpr Vector4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) const noexcept;

	// operators
	[[nodiscard]] constexpr Vector2 operator+() const noexcept;
	[[nodiscard]] constexpr Vector2 operator-() const noexcept;
	[[nodiscard]] constexpr bool operator==(Vector2 const &other) const noexcept = default;
	constexpr Vector2 &operator+=(Vector2 const &other) noexcept;
	constexpr Vector2 &operator+=(f32 other) noexcept;
	constexpr Vector2 &operator-=(Vector2 const &other) noexcept;
	constexpr Vector2 &operator-=(f32 other) noexcept;
	constexpr Vector2 &operator*=(Vector2 const &other) noexcept;
	constexpr Vector2 &operator*=(f32 other) noexcept;
	constexpr Vector2 &operator/=(Vector2 const &other) noexcept;
	constexpr Vector2 &operator/=(f32 other) noexcept;
	constexpr Vector2 &operator=(f32 other) noexcept;
};

struct alignas(16) Vector3 {
	f32 x, y, z;

	// constructors
	[[nodiscard]] constexpr Vector3() noexcept = default;
	[[nodiscard]] constexpr explicit Vector3(f32 n) noexcept;
	[[nodiscard]] constexpr Vector3(f32 x, f32 y, f32 z) noexcept;
	[[nodiscard]] constexpr explicit Vector3(Vector2 const &xy, f32 z = 0.0F) noexcept;
	[[nodiscard]] constexpr Vector3(f32 x, Vector2 const &yz) noexcept;
	[[nodiscard]] constexpr explicit Vector3(Vector4 const &xyz) noexcept;

	// static
	[[nodiscard]] static constexpr Vector3 x_axis(f32 x = 1.0F) noexcept;
	[[nodiscard]] static constexpr Vector3 y_axis(f32 y = 1.0F) noexcept;
	[[nodiscard]] static constexpr Vector3 z_axis(f32 z = 1.0F) noexcept;
	[[nodiscard]] static Vector3 spherical(f32 azimuth, f32 inclination, f32 radius = 1.0F) noexcept;
	[[nodiscard]] static Vector3 cylindrical(f32 angle, f32 radius = 1.0F, f32 height = 0.0F) noexcept;

	// element access
	[[nodiscard]] constexpr f32 &operator[](i32 i) noexcept;
	[[nodiscard]] constexpr f32 const &operator[](i32 i) const noexcept;

	// swizzle
	[[nodiscard]] constexpr Vector2 operator[](i32 i0, i32 i1) noexcept;
	[[nodiscard]] constexpr Vector2 operator[](i32 i0, i32 i1) const noexcept;
	[[nodiscard]] constexpr Vector3 operator[](i32 i0, i32 i1, i32 i2) noexcept;
	[[nodiscard]] constexpr Vector3 operator[](i32 i0, i32 i1, i32 i2) const noexcept;
	[[nodiscard]] constexpr Vector4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) noexcept;
	[[nodiscard]] constexpr Vector4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) const noexcept;

	// operators
	[[nodiscard]] constexpr Vector3 operator+() const noexcept;
	[[nodiscard]] constexpr Vector3 operator-() const noexcept;
	[[nodiscard]] constexpr bool operator==(Vector3 const &other) const noexcept = default;
	constexpr Vector3 &operator+=(Vector3 const &other) noexcept;
	constexpr Vector3 &operator+=(f32 other) noexcept;
	constexpr Vector3 &operator-=(Vector3 const &other) noexcept;
	constexpr Vector3 &operator-=(f32 other) noexcept;
	constexpr Vector3 &operator*=(Vector3 const &other) noexcept;
	constexpr Vector3 &operator*=(f32 other) noexcept;
	constexpr Vector3 &operator/=(Vector3 const &other) noexcept;
	constexpr Vector3 &operator/=(f32 other) noexcept;
	constexpr Vector3 &operator=(f32 other) noexcept;
};

struct alignas(16) Vector4 {
	f32 x, y, z, w;

	// constructors
	[[nodiscard]] constexpr Vector4() noexcept = default;
	[[nodiscard]] constexpr explicit Vector4(f32 n) noexcept;
	[[nodiscard]] constexpr Vector4(f32 x, f32 y, f32 z, f32 w) noexcept;
	[[nodiscard]] constexpr explicit Vector4(Vector2 const &xy) noexcept;
	[[nodiscard]] constexpr Vector4(Vector2 const &xy, f32 z, f32 w) noexcept;
	[[nodiscard]] constexpr Vector4(f32 x, Vector2 const &yz, f32 w) noexcept;
	[[nodiscard]] constexpr Vector4(f32 x, f32 y, Vector2 const &zw) noexcept;
	[[nodiscard]] constexpr Vector4(Vector2 const &xy, Vector2 const &zw) noexcept;
	[[nodiscard]] constexpr explicit Vector4(Vector3 const &xyz, f32 w = 0.0F) noexcept;
	[[nodiscard]] constexpr Vector4(f32 x, Vector3 const &yzw) noexcept;

	// static
	[[nodiscard]] static constexpr Vector4 x_axis(f32 x = 1.0F) noexcept;
	[[nodiscard]] static constexpr Vector4 y_axis(f32 y = 1.0F) noexcept;
	[[nodiscard]] static constexpr Vector4 z_axis(f32 z = 1.0F) noexcept;
	[[nodiscard]] static constexpr Vector4 w_axis(f32 w = 1.0F) noexcept;

	// element access
	[[nodiscard]] constexpr f32 &operator[](i32 i) noexcept;
	[[nodiscard]] constexpr f32 const &operator[](i32 i) const noexcept;

	// swizzle
	[[nodiscard]] constexpr Vector2 operator[](i32 i0, i32 i1) noexcept;
	[[nodiscard]] constexpr Vector2 operator[](i32 i0, i32 i1) const noexcept;
	[[nodiscard]] constexpr Vector3 operator[](i32 i0, i32 i1, i32 i2) noexcept;
	[[nodiscard]] constexpr Vector3 operator[](i32 i0, i32 i1, i32 i2) const noexcept;
	[[nodiscard]] constexpr Vector4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) noexcept;
	[[nodiscard]] constexpr Vector4 operator[](i32 i0, i32 i1, i32 i2, i32 i3) const noexcept;

	// member operators
	[[nodiscard]] constexpr Vector4 operator+() const noexcept;
	[[nodiscard]] constexpr Vector4 operator-() const noexcept;
	[[nodiscard]] constexpr bool operator==(Vector4 const &other) const noexcept = default;
	constexpr Vector4 &operator+=(Vector4 const &other) noexcept;
	constexpr Vector4 &operator+=(f32 other) noexcept;
	constexpr Vector4 &operator-=(Vector4 const &other) noexcept;
	constexpr Vector4 &operator-=(f32 other) noexcept;
	constexpr Vector4 &operator*=(Vector4 const &other) noexcept;
	constexpr Vector4 &operator*=(f32 other) noexcept;
	constexpr Vector4 &operator/=(Vector4 const &other) noexcept;
	constexpr Vector4 &operator/=(f32 other) noexcept;
	constexpr Vector4 &operator=(f32 other) noexcept;
};
} // namespace draco::math

template<typename T> consteval T select(draco::i32 const i, T const v1, T const v2) {
	switch (i) {
	case 0:  return v1;
	case 1:  return v2;
	default: throw "Index out of range";
	}
}

template<typename T> consteval T select(draco::i32 const i, T const v1, T const v2, T const v3) {
	switch (i) {
	case 0:  return v1;
	case 1:  return v2;
	case 2:  return v3;
	default: throw "Index out of range";
	}
}

template<typename T> consteval T select(draco::i32 const i, T const v1, T const v2, T const v3, T const v4) {
	switch (i) {
	case 0:  return v1;
	case 1:  return v2;
	case 2:  return v3;
	case 3:  return v4;
	default: throw "Index out of range";
	}
}
