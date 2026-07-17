export module core.math.transform;

import core.stdtypes;
import core.math.types;
import core.math.matrix4;
import core.math.quaternion;

export namespace draco::math
{
    // =======================================================================
    // Transform - position / rotation / scale, composed as S * R * T.
    // =======================================================================
    struct [[nodiscard]] Transform
    {
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        Quaternion rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
        Vector3 scale{ 1.0f, 1.0f, 1.0f };

        // Identity transform (position 0, rotation identity, scale 1) - the
        // default-constructed value.
        static constexpr Transform identity() noexcept { return {}; }

        Matrix4 toMatrix() const noexcept
        {
            Matrix4 result = Matrix4::scale(scale) * rotationMatrix(rotation);
            result.m[3][0] = position.x;
            result.m[3][1] = position.y;
            result.m[3][2] = position.z;
            return result;
        }

        // Fills a caller-provided 16-float buffer (row-major, matching Matrix4::data()).
        void toMatrix(f32 out[16]) const noexcept
        {
            const Matrix4 m = toMatrix();
            const f32* src = m.data();
            for (usize i = 0; i < 16; ++i) { out[i] = src[i]; }
        }

        // Component-wise interpolation: position/scale lerp, rotation slerp.
        static Transform lerp(const Transform& a, const Transform& b, f32 t) noexcept
        {
            return Transform{
                draco::math::lerp(a.position, b.position, t),
                slerp(a.rotation, b.rotation, t),
                draco::math::lerp(a.scale, b.scale, t),
            };
        }
    };
}
