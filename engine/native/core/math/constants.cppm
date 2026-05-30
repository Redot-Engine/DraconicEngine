module;

#include <numbers>
#include <limits>

export module core.math.constants;
import core.defs;
import core.stdtypes;

export namespace draco::math {
// Limit the depth of recursive algorithms
constexpr int MAX_RECURSIONS = 100;

constexpr f32 SQRT2              = std::numbers::sqrt2_v<f32>;
constexpr f32 SQRT3              = std::numbers::sqrt3_v<f32>;
constexpr f32 SQRT12             = 1. / SQRT2;
constexpr f32 SQRT13             = std::numbers::inv_sqrt3_v<f32>;
constexpr f32 LN2                = std::numbers::ln2_v<f32>;
constexpr f32 LN10               = std::numbers::ln10_v<f32>;
constexpr f32 PI                 = std::numbers::pi_v<f32>;
constexpr f32 PI2                = PI * .5;
constexpr f32 TAU                = 2. * PI;
constexpr f32 E                  = std::numbers::e_v<f32>;
constexpr f32 INF                = std::numeric_limits<f32>::infinity();
constexpr f32 NaN                = std::numeric_limits<f32>::quiet_NaN();
constexpr f32 DB_CONVERSION_GAIN = 8.6858896380650365530225783783321;
constexpr f32 GAIN_CONVERSION_DB = 0.11512925464970228420089957273422;
constexpr u16 UINT16_MAX_VAL     = std::numeric_limits<u16>::max();
constexpr u32 UINT32_MAX_VAL     = std::numeric_limits<u32>::max();
// This is a reciprocal for normalization
// Used to map a u32 [0, MAX] range to a float [0, 1.0] range
// Calculated via double precision to prevent rounding errors
constexpr f32 UINT32_INVERSE_MAX_F = static_cast<f32>(1.0 / static_cast<f64>(UINT32_MAX_VAL));
constexpr f32 DECIMAL_LIMIT_F      = 8388608.0F;

constexpr f32 CMP_EPSILON  = 0.000001F;
constexpr f32 CMP_EPSILON2 = CMP_EPSILON * CMP_EPSILON;

constexpr f32 CMP_NORMALIZE_TOLERANCE    = 0.000001F;
constexpr f32 CMP_NORMALIZE_TOLERANCE2   = CMP_NORMALIZE_TOLERANCE * CMP_NORMALIZE_TOLERANCE;
constexpr f32 CMP_POINT_IN_PLANE_EPSILON = 0.00001F;
} // namespace draco::math
