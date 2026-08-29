// Matrix4 implementation: the 4x4 inverse routines. Kept out of the module interface
// so Matrix4.cppm carries no <cassert>/<cmath>/<limits> - only these bodies need them.
module;

#include <cassert>
#include <cmath>
#include <limits>
#include <optional>

module core.math.matrix4;

import core.stdtypes;
import core.math.constants;

namespace draco::math
{
    std::optional<Matrix4> tryInverse(const Matrix4& mat) noexcept
    {
        const f32* m = &mat.m[0][0];
        f32 inv[16];

        inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
        inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
        inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
        inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
        inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
        inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
        inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
        inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
        inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]  - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
        inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]  + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
        inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]  - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
        inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]  + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];
        inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]  + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7]   + m[9]*m[3]*m[6];
        inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]  - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7]   - m[8]*m[3]*m[6];
        inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]   + m[4]*m[1]*m[11] - m[4]*m[3]*m[9]  - m[8]*m[1]*m[7]   + m[8]*m[3]*m[5];
        inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]   - m[4]*m[1]*m[10] + m[4]*m[2]*m[9]  + m[8]*m[1]*m[6]   - m[8]*m[2]*m[5];

        const f32 det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];

        // Scale-aware singularity: by Hadamard's inequality |det| <= product of the
        // row norms, so reject relative to that product, not an absolute epsilon.
        const auto rowNorm = [&](usize r) {
            const f32* row = m + r * 4;
            return std::sqrt(row[0]*row[0] + row[1]*row[1] + row[2]*row[2] + row[3]*row[3]);
        };
        const f32 normProduct = rowNorm(0) * rowNorm(1) * rowNorm(2) * rowNorm(3);
        if (std::abs(det) <= CMP_EPSILON * normProduct)
        {
            return std::nullopt;
        }

        const f32 invDet = 1.0f / det;
        Matrix4 result{};
        f32* out = &result.m[0][0];
        for (usize i = 0; i < 16; ++i)
        {
            out[i] = inv[i] * invDet;
        }
        return result;
    }

    Matrix4 inverse(const Matrix4& mat) noexcept
    {
        if (const std::optional<Matrix4> inv = tryInverse(mat)) { return *inv; }
        assert(false && "Matrix4 inverse(): singular matrix; use tryInverse() to handle it");
        Matrix4 result{};
        f32* out = &result.m[0][0];
        for (usize i = 0; i < 16; ++i) { out[i] = std::numeric_limits<f32>::quiet_NaN(); }
        return result;
    }
}
