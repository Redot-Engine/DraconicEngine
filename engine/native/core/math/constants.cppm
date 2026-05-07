module;

export module core.math.constants;
import core.defs;

export namespace draco::math {
    // Limit the depth of recursive algorithms
    constexpr int MAX_RECURSIONS = 100;

    constexpr double SQRT2 = 1.414213562373095048801688724209698079;
    constexpr double SQRT3 = 1.732050807568877293527446341505872367;
    constexpr double SQRT12 = 1. / SQRT2;
    constexpr double SQRT13 = 1. / SQRT3;
    constexpr double LN2 = 0.693147180559945309417232121458176568;
    constexpr double LN10 = 2.302585092994045684017991454684364208;
    constexpr double PI = 3.141592653589793238462643383279502884;
    constexpr double TAU = 2. * PI;
    constexpr double E = 2.718281828459045235360287471352662498;
    constexpr double INF = __builtin_huge_val();
    constexpr double NaN = __builtin_nan("");
    constexpr double DB_CONVERSION_GAIN = 8.6858896380650365530225783783321;
    constexpr double GAIN_CONVERSION_DB = 0.11512925464970228420089957273422;
    constexpr double UINT32_MAX_D = 1. / static_cast<double>(__INT_MAX__ * 2U + 1);
    constexpr float  UINT32_MAX_F = 1.f / static_cast<float>(__INT_MAX__ * 2U + 1);

    template<floating_point T> constexpr T CMP_EPSILON = T{0.000001};
    template<floating_point T> constexpr T CMP_EPSILON2 = CMP_EPSILON<T> * CMP_EPSILON<T>;

    template<floating_point T> constexpr T CMP_NORMALIZE_TOLERANCE = T{0.000001};
    template<floating_point T> constexpr T CMP_POINT_IN_PLANE_EPSILON = T{0.00001};
}
