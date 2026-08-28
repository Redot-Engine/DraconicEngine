module;

#include <cmath>

export module core.math.packed;

import core.defs;
import core.stdtypes;
import core.math.types;

export namespace draco::math {

// Packed vector components with no SIMD alignment padding. The VectorN types are
// alignas(16) (a Vector3 occupies 16 bytes), which pads tightly-laid-out data such as
// GPU vertex streams past its canonical stride. Float2/3/4 stay byte-tight and convert
// implicitly to/from the aligned VectorN so math still reads naturally.
struct Float2 {
    f32 x = 0, y = 0;
    constexpr Float2() noexcept = default;
    constexpr Float2(f32 x, f32 y) noexcept : x(x), y(y) {}
    constexpr Float2(Vector2 v) noexcept : x(v.x), y(v.y) {}
    constexpr operator Vector2() const noexcept { return { x, y }; }
    constexpr Float2 operator-(Float2 o) const noexcept { return { x - o.x, y - o.y }; }
    constexpr Float2 operator+(Float2 o) const noexcept { return { x + o.x, y + o.y }; }
};

struct Float3 {
    f32 x = 0, y = 0, z = 0;
    constexpr Float3() noexcept = default;
    constexpr Float3(f32 x, f32 y, f32 z) noexcept : x(x), y(y), z(z) {}
    constexpr Float3(Vector3 v) noexcept : x(v.x), y(v.y), z(v.z) {}
    constexpr operator Vector3() const noexcept { return { x, y, z }; }
    constexpr Float3 operator-(Float3 o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
    constexpr Float3 operator+(Float3 o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
    constexpr Float3 operator*(f32 s)  const noexcept { return { x * s, y * s, z * s }; }
    constexpr Float3& operator+=(Float3 o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
};

struct Float4 {
    f32 x = 0, y = 0, z = 0, w = 0;
    constexpr Float4() noexcept = default;
    constexpr Float4(f32 x, f32 y, f32 z, f32 w) noexcept : x(x), y(y), z(z), w(w) {}
    constexpr Float4(Vector4 v) noexcept : x(v.x), y(v.y), z(v.z), w(v.w) {}
    constexpr operator Vector4() const noexcept { return { x, y, z, w }; }
};

[[nodiscard]] constexpr f32 dot(Float3 a, Float3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
[[nodiscard]] constexpr Float3 cross(Float3 a, Float3 b) noexcept {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
[[nodiscard]] constexpr f32 lengthSquared(Float3 v) noexcept { return dot(v, v); }
[[nodiscard]] inline    f32 length(Float3 v) noexcept { return std::sqrt(dot(v, v)); }
[[nodiscard]] inline Float3 normalize(Float3 v) noexcept {
    const f32 len = length(v);
    return len > 0.0f ? Float3{ v.x / len, v.y / len, v.z / len } : v;
}

} // namespace draco::math
