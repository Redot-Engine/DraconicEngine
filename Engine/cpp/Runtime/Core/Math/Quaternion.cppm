export module core.math.quaternion;

import core.stdtypes;
import core.math.types;
import core.math.matrix4;
import core.math.functions;
import core.math.constants;

// Quaternion: unit quaternion rotation - fromAxisAngle, Hamilton product,
// conjugate/dot/normalize/slerp, rotateVector, and rotationMatrix (-> Matrix4).
// Row-vector convention (v' = v * M), matching Matrix4.

export namespace draco::math
{
    // =======================================================================
    // Quaternion - unit quaternion rotation (x, y, z, w).
    // =======================================================================
    struct [[nodiscard]] Quaternion
    {
        f32 x = 0.0f;
        f32 y = 0.0f;
        f32 z = 0.0f;
        f32 w = 1.0f;

        constexpr Quaternion() noexcept = default;
        constexpr Quaternion(f32 inX, f32 inY, f32 inZ, f32 inW) noexcept : x(inX), y(inY), z(inZ), w(inW) {}

        [[nodiscard]] static Quaternion fromAxisAngle(Vector3 axis, f32 radians) noexcept
        {
            // A zero-length axis is not a rotation; normalize() would divide by zero,
            // so fall back to identity rather than produce a NaN/degenerate quaternion.
            if (nearlyZero(dot(axis, axis)))
            {
                return Quaternion{ 0.0f, 0.0f, 0.0f, 1.0f };
            }
            const f32 half = radians * 0.5f;
            const f32 s = sin(half);
            const Vector3 a = normalize(axis);
            return Quaternion{ a.x * s, a.y * s, a.z * s, cos(half) };
        }

        [[nodiscard]] static constexpr Quaternion identity() noexcept { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
    };

    // Hamilton product: applies `b` then `a` to a vector.
    [[nodiscard]] constexpr Quaternion operator*(Quaternion a, Quaternion b) noexcept
    {
        return { a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                 a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                 a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                 a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z };
    }

    [[nodiscard]] constexpr Quaternion conjugate(Quaternion q) noexcept { return { -q.x, -q.y, -q.z, q.w }; }
    [[nodiscard]] constexpr f32 dot(Quaternion a, Quaternion b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

    // General inverse (= conjugate / |q|^2). For unit quaternions this equals the conjugate.
    [[nodiscard]] inline Quaternion inverse(Quaternion q) noexcept
    {
        const f32 lengthSq = dot(q, q);
        if (lengthSq <= CMP_EPSILON * CMP_EPSILON) { return Quaternion::identity(); }
        const f32 inv = 1.0f / lengthSq;
        return { -q.x * inv, -q.y * inv, -q.z * inv, q.w * inv };
    }

    [[nodiscard]] inline Quaternion normalize(Quaternion q) noexcept
    {
        const f32 lengthSq = dot(q, q);
        if (lengthSq <= CMP_EPSILON * CMP_EPSILON) { return Quaternion::identity(); }
        const f32 inv = 1.0f / sqrt(lengthSq);
        return { q.x * inv, q.y * inv, q.z * inv, q.w * inv };
    }

    [[nodiscard]] constexpr Vector3 rotateVector(Quaternion q, Vector3 v) noexcept
    {
        const Vector3 u{ q.x, q.y, q.z };
        const f32 s = q.w;
        return u * (2.0f * dot(u, v)) + v * (s * s - dot(u, u)) + cross(u, v) * (2.0f * s);
    }

    [[nodiscard]] inline bool nearlyEqual(Quaternion a, Quaternion b, f32 epsilon = CMP_EPSILON) noexcept
    {
        const bool same = nearlyEqual(a.x, b.x, epsilon) && nearlyEqual(a.y, b.y, epsilon)
            && nearlyEqual(a.z, b.z, epsilon) && nearlyEqual(a.w, b.w, epsilon);
        // q and -q represent the same rotation (and slerp/normalize may flip the sign),
        // so accept the antipodal quaternion as equal too.
        const bool antipodal = nearlyEqual(a.x, -b.x, epsilon) && nearlyEqual(a.y, -b.y, epsilon)
            && nearlyEqual(a.z, -b.z, epsilon) && nearlyEqual(a.w, -b.w, epsilon);
        return same || antipodal;
    }

    // Spherical linear interpolation along the shortest arc; result is unit.
    [[nodiscard]] inline Quaternion slerp(Quaternion a, Quaternion b, f32 t) noexcept
    {
        f32 cosTheta = dot(a, b);
        if (cosTheta < 0.0f) // shortest path
        {
            b = Quaternion{ -b.x, -b.y, -b.z, -b.w };
            cosTheta = -cosTheta;
        }

        if (cosTheta > 0.9995f) // nearly parallel - lerp + normalize
        {
            return normalize(Quaternion{ a.x + (b.x - a.x) * t,
                                         a.y + (b.y - a.y) * t,
                                         a.z + (b.z - a.z) * t,
                                         a.w + (b.w - a.w) * t });
        }

        const f32 theta0 = acos(cosTheta);
        const f32 theta = theta0 * t;
        const f32 sinTheta = sin(theta);
        const f32 sinTheta0 = sin(theta0);
        const f32 s1 = sinTheta / sinTheta0;
        const f32 s0 = cos(theta) - cosTheta * s1;
        return Quaternion{ a.x * s0 + b.x * s1,
                           a.y * s0 + b.y * s1,
                           a.z * s0 + b.z * s1,
                           a.w * s0 + b.w * s1 };
    }

    // Rotation matrix for a unit quaternion (row-vector convention).
    [[nodiscard]] constexpr Matrix4 rotationMatrix(Quaternion q) noexcept
    {
        const f32 xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        const f32 xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
        const f32 wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
        return Matrix4{ { { 1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),        2.0f * (xz - wy),        0.0f },
                          { 2.0f * (xy - wz),        1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx),        0.0f },
                          { 2.0f * (xz + wy),        2.0f * (yz - wx),        1.0f - 2.0f * (xx + yy), 0.0f },
                          { 0.0f,                    0.0f,                    0.0f,                    1.0f } } };
    }
}
