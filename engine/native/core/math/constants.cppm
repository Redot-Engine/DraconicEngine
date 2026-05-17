module;

#include <numbers>
#include <limits>

export module core.math.constants;
import core.defs;

export namespace draco::math {
    // Limit the depth of recursive algorithms
    constexpr int MAX_RECURSIONS = 100;

    constexpr float SQRT2 = std::numbers::sqrt2_v<float>;
    constexpr float SQRT3 = std::numbers::sqrt3_v<float>;
    constexpr float SQRT12 = 1. / SQRT2;
    constexpr float SQRT13 = std::numbers::inv_sqrt3_v<float>;
    constexpr float LN2 = std::numbers::ln2_v<float>;
    constexpr float LN10 = std::numbers::ln10_v<float>;
    constexpr float PI = std::numbers::pi_v<float>;
    constexpr float PI2 = PI * .5;
    constexpr float TAU = 2. * PI;
    constexpr float E = std::numbers::e_v<float>;
    constexpr float INF = std::numeric_limits<float>::infinity();
    constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
    constexpr float DB_CONVERSION_GAIN = 8.6858896380650365530225783783321;
    constexpr float GAIN_CONVERSION_DB = 0.11512925464970228420089957273422;
    constexpr float UINT32_MAX_F = 1.f / std::numeric_limits<uint32_t>::max();
    constexpr float DECIMAL_LIMIT_F = 8388608.0f;

    constexpr float CMP_EPSILON = 0.000001f;
    constexpr float CMP_EPSILON2 = CMP_EPSILON * CMP_EPSILON;

    constexpr float CMP_NORMALIZE_TOLERANCE = 0.000001f;
    constexpr float CMP_NORMALIZE_TOLERANCE2 = CMP_NORMALIZE_TOLERANCE * CMP_NORMALIZE_TOLERANCE;
    constexpr float CMP_POINT_IN_PLANE_EPSILON = 0.00001f;
}
