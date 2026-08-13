module;

#include <optional>   // tryInverse() return type; the rest of the impl lives in Matrix4.cpp

export module core.math.matrix4;

import core.stdtypes;
import core.math.types;
import core.math.functions;
import core.math.constants;

// Matrix4: 4x4 row-major matrix - transforms, projections (perspective/ortho/
// lookAt RH), multiply, transpose/determinant/inverse, point/direction xform.
//
// Conventions: row-major storage m[row][col]; row vectors (v' = v * M);
// composition left-to-right; right-handed projections, NDC depth [0,1];
// translation in the last row.

export namespace draco::math
{
    // =======================================================================
    // Matrix4 - 4x4, row-major, row-vector convention.
    // =======================================================================
    struct [[nodiscard]] Matrix4
    {
        f32 m[4][4];

        // Raw row-major float pointer (16 contiguous floats), e.g. for GPU upload.
        [[nodiscard]] const f32* data() const noexcept { return &m[0][0]; }
        [[nodiscard]] f32* data() noexcept { return &m[0][0]; }

        [[nodiscard]] constexpr f32 operator[](usize row, usize col) const noexcept
        {
            return m[row][col];
        }
        [[nodiscard]] constexpr f32& operator[](usize row, usize col) noexcept
        {
            return m[row][col];
        }

        [[nodiscard]] static constexpr Matrix4 identity() noexcept
        {
            return Matrix4{ { { 1.0f, 0.0f, 0.0f, 0.0f },
                              { 0.0f, 1.0f, 0.0f, 0.0f },
                              { 0.0f, 0.0f, 1.0f, 0.0f },
                              { 0.0f, 0.0f, 0.0f, 1.0f } } };
        }

        [[nodiscard]] static constexpr Matrix4 translation(Vector3 t) noexcept
        {
            return Matrix4{ { { 1.0f, 0.0f, 0.0f, 0.0f },
                              { 0.0f, 1.0f, 0.0f, 0.0f },
                              { 0.0f, 0.0f, 1.0f, 0.0f },
                              { t.x,  t.y,  t.z,  1.0f } } };
        }

        [[nodiscard]] static constexpr Matrix4 scale(Vector3 s) noexcept
        {
            return Matrix4{ { { s.x,  0.0f, 0.0f, 0.0f },
                              { 0.0f, s.y,  0.0f, 0.0f },
                              { 0.0f, 0.0f, s.z,  0.0f },
                              { 0.0f, 0.0f, 0.0f, 1.0f } } };
        }

        [[nodiscard]] static Matrix4 rotationX(f32 radians) noexcept
        {
            const f32 c = cos(radians);
            const f32 s = sin(radians);
            return Matrix4{ { { 1.0f, 0.0f, 0.0f, 0.0f },
                              { 0.0f, c,    s,    0.0f },
                              { 0.0f, -s,   c,    0.0f },
                              { 0.0f, 0.0f, 0.0f, 1.0f } } };
        }

        [[nodiscard]] static Matrix4 rotationY(f32 radians) noexcept
        {
            const f32 c = cos(radians);
            const f32 s = sin(radians);
            return Matrix4{ { { c,    0.0f, -s,   0.0f },
                              { 0.0f, 1.0f, 0.0f, 0.0f },
                              { s,    0.0f, c,    0.0f },
                              { 0.0f, 0.0f, 0.0f, 1.0f } } };
        }

        [[nodiscard]] static Matrix4 rotationZ(f32 radians) noexcept
        {
            const f32 c = cos(radians);
            const f32 s = sin(radians);
            return Matrix4{ { { c,    s,    0.0f, 0.0f },
                              { -s,   c,    0.0f, 0.0f },
                              { 0.0f, 0.0f, 1.0f, 0.0f },
                              { 0.0f, 0.0f, 0.0f, 1.0f } } };
        }

        // Right-handed perspective, NDC z in [0, 1].
        [[nodiscard]] static Matrix4 perspectiveFovRH(f32 fovYRadians, f32 aspect, f32 zNear, f32 zFar) noexcept
        {
            const f32 yScale = 1.0f / tan(fovYRadians * 0.5f);
            const f32 xScale = yScale / aspect;
            const f32 zRange = zFar / (zNear - zFar);
            return Matrix4{ { { xScale, 0.0f,   0.0f,           0.0f },
                              { 0.0f,   yScale, 0.0f,           0.0f },
                              { 0.0f,   0.0f,   zRange,         -1.0f },
                              { 0.0f,   0.0f,   zNear * zRange, 0.0f } } };
        }

        [[nodiscard]] static Matrix4 orthographicRH(f32 width, f32 height, f32 zNear, f32 zFar) noexcept
        {
            const f32 zRange = 1.0f / (zNear - zFar);
            return Matrix4{ { { 2.0f / width, 0.0f,          0.0f,           0.0f },
                              { 0.0f,         2.0f / height, 0.0f,           0.0f },
                              { 0.0f,         0.0f,          zRange,         0.0f },
                              { 0.0f,         0.0f,          zNear * zRange, 1.0f } } };
        }

        [[nodiscard]] static Matrix4 lookAtRH(Vector3 eye, Vector3 target, Vector3 up) noexcept
        {
            const Vector3 zAxis = normalize(eye - target); // camera looks down -z
            const Vector3 xAxis = normalize(cross(up, zAxis));
            const Vector3 yAxis = cross(zAxis, xAxis);
            return Matrix4{ { { xAxis.x, yAxis.x, zAxis.x, 0.0f },
                              { xAxis.y, yAxis.y, zAxis.y, 0.0f },
                              { xAxis.z, yAxis.z, zAxis.z, 0.0f },
                              { -dot(xAxis, eye), -dot(yAxis, eye), -dot(zAxis, eye), 1.0f } } };
        }
    };

    [[nodiscard]] constexpr Matrix4 operator*(const Matrix4& a, const Matrix4& b) noexcept
    {
        Matrix4 result{};
        for (usize row = 0; row < 4; ++row)
        {
            for (usize col = 0; col < 4; ++col)
            {
                f32 sum = 0.0f;
                for (usize k = 0; k < 4; ++k)
                {
                    sum += a.m[row][k] * b.m[k][col];
                }
                result.m[row][col] = sum;
            }
        }
        return result;
    }

    // Row-vector transform: v' = v * M.
    [[nodiscard]] constexpr Vector4 operator*(Vector4 v, const Matrix4& m) noexcept
    {
        return { v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0],
                 v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1],
                 v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2],
                 v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3] };
    }

    [[nodiscard]] constexpr Matrix4 transpose(const Matrix4& a) noexcept
    {
        Matrix4 result{};
        for (usize row = 0; row < 4; ++row)
        {
            for (usize col = 0; col < 4; ++col)
            {
                result.m[row][col] = a.m[col][row];
            }
        }
        return result;
    }

    // Transforms a position (implicit w = 1, translation applied).
    [[nodiscard]] constexpr Vector3 transformPoint(Vector3 p, const Matrix4& m) noexcept
    {
        return { p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0],
                 p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1],
                 p.x * m.m[0][2] + p.y * m.m[1][2] + p.z * m.m[2][2] + m.m[3][2] };
    }

    // Transforms a direction (implicit w = 0, translation ignored).
    [[nodiscard]] constexpr Vector3 transformDirection(Vector3 d, const Matrix4& m) noexcept
    {
        return { d.x * m.m[0][0] + d.y * m.m[1][0] + d.z * m.m[2][0],
                 d.x * m.m[0][1] + d.y * m.m[1][1] + d.z * m.m[2][1],
                 d.x * m.m[0][2] + d.y * m.m[1][2] + d.z * m.m[2][2] };
    }

    // Transforms a 2D position (implicit z = 0, w = 1, translation applied;
    // result projected back to 2D). For 2D affine transforms stored in a Matrix4.
    [[nodiscard]] constexpr Vector2 transformPoint2D(Vector2 p, const Matrix4& m) noexcept
    {
        return { p.x * m.m[0][0] + p.y * m.m[1][0] + m.m[3][0],
                 p.x * m.m[0][1] + p.y * m.m[1][1] + m.m[3][1] };
    }

    // Exact element-wise equality (e.g. for an identity fast-path).
    [[nodiscard]] constexpr bool operator==(const Matrix4& a, const Matrix4& b) noexcept
    {
        for (usize row = 0; row < 4; ++row)
            for (usize col = 0; col < 4; ++col)
                if (a.m[row][col] != b.m[row][col]) { return false; }
        return true;
    }

    [[nodiscard]] constexpr bool nearlyEqual(const Matrix4& a, const Matrix4& b, f32 epsilon = CMP_EPSILON) noexcept
    {
        for (usize row = 0; row < 4; ++row)
        {
            for (usize col = 0; col < 4; ++col)
            {
                if (!nearlyEqual(a.m[row][col], b.m[row][col], epsilon)) { return false; }
            }
        }
        return true;
    }

    [[nodiscard]] inline f32 determinant(const Matrix4& mat) noexcept
    {
        const f32* m = &mat.m[0][0];
        const f32 s0 = m[0] * m[5] - m[1] * m[4];
        const f32 s1 = m[0] * m[6] - m[2] * m[4];
        const f32 s2 = m[0] * m[7] - m[3] * m[4];
        const f32 s3 = m[1] * m[6] - m[2] * m[5];
        const f32 s4 = m[1] * m[7] - m[3] * m[5];
        const f32 s5 = m[2] * m[7] - m[3] * m[6];
        const f32 c5 = m[10] * m[15] - m[11] * m[14];
        const f32 c4 = m[9] * m[15] - m[11] * m[13];
        const f32 c3 = m[9] * m[14] - m[10] * m[13];
        const f32 c2 = m[8] * m[15] - m[11] * m[12];
        const f32 c1 = m[8] * m[14] - m[10] * m[12];
        const f32 c0 = m[8] * m[13] - m[9] * m[12];
        return s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
    }

    // Full 4x4 inverse (adjugate / determinant). Returns nullopt for a singular
    // matrix, judged with a scale-aware (Hadamard row-norm) threshold so a small-
    // but-invertible matrix (e.g. a 1e-3 uniform scale) is not wrongly rejected.
    // (Implementation in Matrix4.cpp.)
    [[nodiscard]] std::optional<Matrix4> tryInverse(const Matrix4& mat) noexcept;

    // Convenience inverse for matrices known to be invertible. On a singular matrix it
    // does NOT silently return identity (which would hide the failure) - it asserts in
    // debug and returns a not-a-number matrix so the error propagates visibly. Prefer
    // tryInverse() whenever a matrix may be singular. (Implementation in Matrix4.cpp.)
    [[nodiscard]] Matrix4 inverse(const Matrix4& mat) noexcept;
}
