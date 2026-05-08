module;

#include <concepts>
#include <cstdint>
#include <limits>
#include <numbers>

export module core.math.constants;
import core.defs;

export namespace draco::math {
    // Limit the depth of recursive algorithms
    constexpr int MAX_RECURSIONS = 100;

    constexpr double SQRT2 = std::numbers::sqrt2_v<double>;
    constexpr double SQRT3 = std::numbers::sqrt3_v<double>;
    constexpr double SQRT12 = 1. / SQRT2;
    constexpr double SQRT13 = std::numbers::inv_sqrt3_v<double>;
    constexpr double LN2 = std::numbers::ln2_v<double>;
    constexpr double LN10 = std::numbers::ln10_v<double>;
    constexpr double PI = std::numbers::pi_v<double>;
    constexpr double TAU = 2. * PI;
    constexpr double E = std::numbers::e_v<double>;
    constexpr double INF = std::numeric_limits<double>::infinity();
    constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
    constexpr double DB_CONVERSION_GAIN = 8.6858896380650365530225783783321;
    constexpr double GAIN_CONVERSION_DB = 0.11512925464970228420089957273422;
    constexpr double UINT32_MAX_D = 1. / std::numeric_limits<uint32_t>::max();
    constexpr float  UINT32_MAX_F = 1.f / std::numeric_limits<uint32_t>::max();

    template<std::floating_point T> constexpr T CMP_EPSILON = T{0.000001};
    template<std::floating_point T> constexpr T CMP_EPSILON2 = CMP_EPSILON<T> * CMP_EPSILON<T>;

    template<std::floating_point T> constexpr T CMP_NORMALIZE_TOLERANCE = T{0.000001};
    template<std::floating_point T> constexpr T CMP_POINT_IN_PLANE_EPSILON = T{0.00001};
}
