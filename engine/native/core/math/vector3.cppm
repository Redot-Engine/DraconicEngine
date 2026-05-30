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
import core.stdtypes;

export namespace draco::math {
// assertions
static_assert(sizeof(Vector3) == 16, "Vector3 must be 16 bytes");
static_assert(alignof(Vector3) == 16, "Vector3 must be 16-byte aligned");
static_assert(trivial<Vector3>, "Vector3 must be trivial");
static_assert(std::is_standard_layout_v<Vector3>,
              "Vector3 must be standard layout");

// constructors
[[nodiscard]] constexpr Vector3::Vector3(f32 const n) noexcept :
	x{n}, y{n}, z{n} { }

[[nodiscard]] constexpr Vector3::Vector3(f32 const x,
                                         f32 const y,
                                         f32 const z) noexcept :
	x{x}, y{y}, z{z} { }

[[nodiscard]] constexpr Vector3::Vector3(Vector2 const &xy,
                                         f32 const z) noexcept :
	x{xy.x}, y{xy.y}, z{z} { }

[[nodiscard]] constexpr Vector3::Vector3(f32 const x,
                                         Vector2 const &yz) noexcept :
	x{x}, y{yz.x}, z{yz.y} { }

[[nodiscard]] constexpr Vector3::Vector3(Vector4 const &xyz) noexcept :
	x{xyz.x}, y{xyz.y}, z{xyz.z} { }

// static
[[nodiscard]] constexpr Vector3 Vector3::x_axis(f32 const x) noexcept {
	return {x, 0.0F, 0.0F};
}

[[nodiscard]] constexpr Vector3 Vector3::y_axis(f32 const y) noexcept {
	return {0.0F, y, 0.0F};
}

[[nodiscard]] constexpr Vector3 Vector3::z_axis(f32 const z) noexcept {
	return {0.0F, 0.0F, z};
}

[[nodiscard]] Vector3 Vector3::spherical(f32 const azimuth,
                                         f32 const inclination,
                                         f32 const radius) noexcept {
	f32 const sin_incl = radius * std::sin(inclination);
	return {sin_incl * std::cos(azimuth), radius * std::cos(inclination),
	        sin_incl * std::sin(azimuth)};
}

[[nodiscard]] Vector3 Vector3::cylindrical(f32 const angle,
                                           f32 const radius,
                                           f32 const height) noexcept {
	return {radius * std::cos(angle), height, radius * std::sin(angle)};
}

// element access
[[nodiscard]] constexpr f32 &Vector3::operator[](i32 const i) noexcept {
	if consteval {
		switch (i) {
		case 0:  return x;
		case 1:  return y;
		default:
		case 2:  return z;
		}
	}
	else { return (&x)[i]; }
}

[[nodiscard]] constexpr f32 const &
Vector3::operator[](i32 const i) const noexcept {
	if consteval {
		switch (i) {
		case 0:  return x;
		case 1:  return y;
		default:
		case 2:  return z;
		}
	}
	else { return (&x)[i]; }
}

// swizzle
[[nodiscard]] constexpr Vector2 Vector3::operator[](i32 const i0,
                                                    i32 const i1) noexcept {
	if consteval { return {select(i0, x, y, z), select(i1, x, y, z)}; }
	else { return {(&x)[i0], (&x)[i1]}; }
}

[[nodiscard]] constexpr Vector2
Vector3::operator[](i32 const i0, i32 const i1) const noexcept {
	if consteval { return {select(i0, x, y, z), select(i1, x, y, z)}; }
	else { return {(&x)[i0], (&x)[i1]}; }
}

[[nodiscard]] constexpr Vector3
Vector3::operator[](i32 const i0, i32 const i1, i32 const i2) noexcept {
	if consteval {
		return {select(i0, x, y, z), select(i1, x, y, z), select(i2, x, y, z)};
	}
	else { return {(&x)[i0], (&x)[i1], (&x)[i2]}; }
}

[[nodiscard]] constexpr Vector3
Vector3::operator[](i32 const i0, i32 const i1, i32 const i2) const noexcept {
	if consteval {
		return {select(i0, x, y, z), select(i1, x, y, z), select(i2, x, y, z)};
	}
	else { return {(&x)[i0], (&x)[i1], (&x)[i2]}; }
}

[[nodiscard]] constexpr Vector4 Vector3::operator[](i32 const i0,
                                                    i32 const i1,
                                                    i32 const i2,
                                                    i32 const i3) noexcept {
	if consteval {
		return {select(i0, x, y, z), select(i1, x, y, z), select(i2, x, y, z),
		        select(i3, x, y, z)};
	}
	else { return {(&x)[i0], (&x)[i1], (&x)[i2], (&x)[i3]}; }
}

[[nodiscard]] constexpr Vector4 Vector3::operator[](
	i32 const i0, i32 const i1, i32 const i2, i32 const i3
) const noexcept {
	if consteval {
		return {select(i0, x, y, z), select(i1, x, y, z), select(i2, x, y, z),
		        select(i3, x, y, z)};
	}
	else { return {(&x)[i0], (&x)[i1], (&x)[i2], (&x)[i3]}; }
}

// operators
constexpr Vector3 &Vector3::operator+=(Vector3 const &other) noexcept {
	x += other.x;
	y += other.y;
	z += other.z;
	return *this;
}

constexpr Vector3 &Vector3::operator+=(f32 const other) noexcept {
	x += other;
	y += other;
	z += other;
	return *this;
}

constexpr Vector3 &Vector3::operator-=(Vector3 const &other) noexcept {
	x -= other.x;
	y -= other.y;
	z -= other.z;
	return *this;
}

constexpr Vector3 &Vector3::operator-=(f32 const other) noexcept {
	x -= other;
	y -= other;
	z -= other;
	return *this;
}

constexpr Vector3 &Vector3::operator*=(Vector3 const &other) noexcept {
	x *= other.x;
	y *= other.y;
	z *= other.z;
	return *this;
}

constexpr Vector3 &Vector3::operator*=(f32 const other) noexcept {
	x *= other;
	y *= other;
	z *= other;
	return *this;
}

constexpr Vector3 &Vector3::operator/=(Vector3 const &other) noexcept {
	x /= other.x;
	y /= other.y;
	z /= other.z;
	return *this;
}

constexpr Vector3 &Vector3::operator/=(f32 const other) noexcept {
	f32 const inv = 1.0F / other;
	x *= inv;
	y *= inv;
	z *= inv;
	return *this;
}

constexpr Vector3 &Vector3::operator=(f32 const other) noexcept {
	x = other;
	y = other;
	z = other;
	return *this;
}

[[nodiscard]] constexpr Vector3 Vector3::operator+() const noexcept {
	return {x, y, z};
}

[[nodiscard]] constexpr Vector3 Vector3::operator-() const noexcept {
	return {-x, -y, -z};
}

[[nodiscard]] constexpr Vector3 operator+(Vector3 const &a,
                                          Vector3 const &b) noexcept {
	return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] constexpr Vector3 operator+(Vector3 const &a,
                                          f32 const b) noexcept {
	return {a.x + b, a.y + b, a.z + b};
}

[[nodiscard]] constexpr Vector3 operator+(f32 const a,
                                          Vector3 const &b) noexcept {
	return b + a;
}

[[nodiscard]] constexpr Vector3 operator-(Vector3 const &a,
                                          Vector3 const &b) noexcept {
	return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] constexpr Vector3 operator-(Vector3 const &a,
                                          f32 const b) noexcept {
	return {a.x - b, a.y - b, a.z - b};
}

[[nodiscard]] constexpr Vector3 operator-(f32 const a,
                                          Vector3 const &b) noexcept {
	return {a - b.x, a - b.y, a - b.z};
}

[[nodiscard]] constexpr Vector3 operator*(Vector3 const &a,
                                          Vector3 const &b) noexcept {
	return {a.x * b.x, a.y * b.y, a.z * b.z};
}

[[nodiscard]] constexpr Vector3 operator*(Vector3 const &a,
                                          f32 const b) noexcept {
	return {a.x * b, a.y * b, a.z * b};
}

[[nodiscard]] constexpr Vector3 operator*(f32 const a,
                                          Vector3 const &b) noexcept {
	return b * a;
}

[[nodiscard]] constexpr Vector3 operator/(Vector3 const &a,
                                          Vector3 const &b) noexcept {
	return {a.x / b.x, a.y / b.y, a.z / b.z};
}

[[nodiscard]] constexpr Vector3 operator/(Vector3 const &a,
                                          f32 const b) noexcept {
	return a * (1.0F / b);
}

[[nodiscard]] constexpr Vector3 operator/(f32 const a,
                                          Vector3 const &b) noexcept {
	return {a / b.x, a / b.y, a / b.z};
}

// functions

// Returns dot product
[[nodiscard]] constexpr f32 dot(Vector3 const &a, Vector3 const &b) noexcept {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Returns squared magnitude
[[nodiscard]] constexpr f32 length_sq(Vector3 const &v) noexcept {
	return dot(v, v);
}

// Returns magnitude
[[nodiscard]] f32 length(Vector3 const &v) noexcept {
	return std::sqrt(length_sq(v));
}

// Return squared distance between two vectors
[[nodiscard]] constexpr f32 distance_sq(Vector3 const &a,
                                        Vector3 const &b) noexcept {
	return length_sq(a - b);
}

// Returns distance between two vectors
[[nodiscard]] f32 distance(Vector3 const &a, Vector3 const &b) noexcept {
	return length(a - b);
}

// Safe normalize, checks length
[[nodiscard]] Vector3 normalize(Vector3 const &v) noexcept {
	f32 const len = length(v);

	return (len > CMP_NORMALIZE_TOLERANCE) ? v / len : Vector3();
}

// Faster normalize, it presupposes vector has non-zero length
// TODO: add check that v is non-zero on debug builds
[[nodiscard]] Vector3 normalize_fast(Vector3 const &v) noexcept {
	return v / length(v);
}

// Returns vector projected onto normal
[[nodiscard]] constexpr Vector3 project(Vector3 const &vector,
                                        Vector3 const &normal) noexcept {
	return normal * (dot(vector, normal) / length_sq(normal));
}

// Returns a vector reflected off a plane defined by its normal
[[nodiscard]] constexpr Vector3 reflect(Vector3 const &incoming,
                                        Vector3 const &normal) noexcept {
	return incoming - 2.0F * dot(incoming, normal) * normal;
}

// Returns the angle between two vectors
[[nodiscard]] f32 angle(Vector3 const &a, Vector3 const &b) noexcept {
	return std::acos(dot(a, b) / (length(a) * length(b)));
}

// Returns linear interpolation between two vectors
[[nodiscard]] constexpr Vector3
lerp(Vector3 const &from, Vector3 const &to, f32 const weight) noexcept {
	return {lerp(from.x, to.x, weight), lerp(from.y, to.y, weight),
	        lerp(from.z, to.z, weight)};
}

// Returns component-wise minimum
[[nodiscard]] constexpr Vector3 min(Vector3 const &a,
                                    Vector3 const &b) noexcept {
	return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

[[nodiscard]] constexpr Vector3 min(Vector3 const &a, f32 const b) noexcept {
	return {std::min(a.x, b), std::min(a.y, b), std::min(a.z, b)};
}

[[nodiscard]] constexpr Vector3 min(f32 const a, Vector3 const &b) noexcept {
	return min(b, a);
}

// Returns the vector with the smaller length
[[nodiscard]] constexpr Vector3 min_length(Vector3 const &a,
                                           Vector3 const &b) noexcept {
	return length_sq(a) < length_sq(b) ? a : b;
}

// Returns a vector in the same direction whose length is bounded above by the given value.
[[nodiscard]] Vector3 min_length(Vector3 const &a, f32 const b) noexcept {
	f32 const len_sq = length_sq(a);

	if (len_sq > b * b) { return a * (b / std::sqrt(len_sq)); }
	else { return a; }
}

[[nodiscard]] Vector3 min_length(f32 const a, Vector3 const &b) noexcept {
	return min_length(b, a);
}

// Returns component-wise maximum
[[nodiscard]] constexpr Vector3 max(Vector3 const &a,
                                    Vector3 const &b) noexcept {
	return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

[[nodiscard]] constexpr Vector3 max(Vector3 const &a, f32 const b) noexcept {
	return {std::max(a.x, b), std::max(a.y, b), std::max(a.z, b)};
}

[[nodiscard]] constexpr Vector3 max(f32 const a, Vector3 const &b) noexcept {
	return max(b, a);
}

// Returns the vector with the larger length
[[nodiscard]] constexpr Vector3 max_length(Vector3 const &a,
                                           Vector3 const &b) noexcept {
	return length_sq(a) > length_sq(b) ? a : b;
}

// Returns a vector in the same direction whose length is bounded below by the given value. Returns the 0 vector if the vector is too small to be normalized.
[[nodiscard]] Vector3 max_length(Vector3 const &a, f32 const b) noexcept {
	f32 const len_sq = length_sq(a);

	if (len_sq <= CMP_NORMALIZE_TOLERANCE2) { return Vector3(); }
	else if (len_sq < b * b) { return a * (b / std::sqrt(len_sq)); }
	else { return a; }
}

[[nodiscard]] Vector3 max_length(f32 const a, Vector3 const &b) noexcept {
	return max_length(b, a);
}

// Clamps each component of x to the range [x_min, x_max]. Presupposes x_min <= x_max.
[[nodiscard]] constexpr Vector3
clamp(Vector3 const &x, Vector3 const &x_min, Vector3 const &x_max) noexcept {
	return max(x_min, min(x, x_max));
}

[[nodiscard]] constexpr Vector3
clamp(Vector3 const &x, f32 const x_min, f32 const x_max) noexcept {
	return max(x_min, min(x, x_max));
}

// Clamps the length of the vector to the range [x_min, x_max]. Presupposes x_min <= x_max. Returns the 0 vector if the vector is too small to be normalized.
[[nodiscard]] Vector3
clamp_length(Vector3 const &v, f32 const x_min, f32 const x_max) noexcept {
	f32 const len_sq = length_sq(v);

	if (len_sq <= CMP_NORMALIZE_TOLERANCE2) { return Vector3(); }
	else if (len_sq < x_min * x_min) { return v * (x_min / std::sqrt(len_sq)); }
	else if (len_sq > x_max * x_max) { return v * (x_max / std::sqrt(len_sq)); }
	else { return v; }
}

// Returns component-wise absolute value
[[nodiscard]] constexpr Vector3 abs(Vector3 const &v) noexcept {
	return {abs(v.x), abs(v.y), abs(v.z)};
}

// Returns component-wise floor
[[nodiscard]] constexpr Vector3 floor(Vector3 const &v) noexcept {
	return {floor(v.x), floor(v.y), floor(v.z)};
}

// Returns component-wise ceiling
[[nodiscard]] constexpr Vector3 ceil(Vector3 const &v) noexcept {
	return {ceil(v.x), ceil(v.y), ceil(v.z)};
}

// Returns component-wise truncation
[[nodiscard]] constexpr Vector3 trunc(Vector3 const &v) noexcept {
	return {trunc(v.x), trunc(v.y), trunc(v.z)};
}

// Returns component-wise round
[[nodiscard]] constexpr Vector3 round(Vector3 const &v) noexcept {
	return {round(v.x), round(v.y), round(v.z)};
}

// Returns component-wise sign. Note that -0 still returns 0
[[nodiscard]] constexpr Vector3 sign(Vector3 const &v) noexcept {
	return {sign(v.x), sign(v.y), sign(v.z)};
}

// Returns true if the vectors are approximately equal
[[nodiscard]] constexpr bool approx_eq(Vector3 const &a,
                                       Vector3 const &b) noexcept {
	return distance_sq(a, b) < CMP_EPSILON2;
}

// Returns cross product
[[nodiscard]] constexpr Vector3 cross(Vector3 const &a,
                                      Vector3 const &b) noexcept {
	return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
	        a.x * b.y - a.y * b.x};
}
} // namespace draco::math

export namespace std {
template<> struct formatter<draco::math::Vector3> : formatter<draco::f32> {
	auto format(draco::math::Vector3 const &v, format_context &ctx) const {
		ctx.advance_to(format_to(ctx.out(), "{{"));

		for (draco::i32 i = 0; i < 3; ++i) {
			if (i) { ctx.advance_to(format_to(ctx.out(), ", ")); }
			ctx.advance_to(formatter<draco::f32>::format(v[i], ctx));
		}

		return format_to(ctx.out(), "}}");
	}
};
} // namespace std
