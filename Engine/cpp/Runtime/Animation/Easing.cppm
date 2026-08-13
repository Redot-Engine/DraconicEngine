/// Easing types (`:easing` partition).
///
/// EasingType: a serializable enum mapping 1:1 to the core easing functions (core.math.easings).
/// The functions themselves live in core math;
/// this is the animation-facing enum + lookup.

module;

export module animation:easing;

import core;

using namespace draco;

export namespace draco::animation {

// Serializable easing type (1:1 with the core Easings family). Order matters - it's the serialized
// value and several tools index by it.
enum class EasingType : i32 {
    Linear = 0,
    EaseInQuadratic, EaseOutQuadratic, EaseInOutQuadratic,
    EaseInCubic,     EaseOutCubic,     EaseInOutCubic,
    EaseInQuartic,   EaseOutQuartic,   EaseInOutQuartic,
    EaseInQuintic,   EaseOutQuintic,   EaseInOutQuintic,
    EaseInSin,       EaseOutSin,       EaseInOutSin,
    EaseInExponential, EaseOutExponential, EaseInOutExponential,
    EaseInCircular,  EaseOutCircular,  EaseInOutCircular,
    EaseInBack,      EaseOutBack,      EaseInOutBack,
    EaseInElastic,   EaseOutElastic,   EaseInOutElastic,
    EaseInBounce,    EaseOutBounce,    EaseInOutBounce,
    Count
};

// Maps EasingType -> the corresponding core easing function (never null).
[[nodiscard]] inline math::EasingFunction toFunction(EasingType type) noexcept {
    switch (type) {
    case EasingType::Linear:              return math::easeInLinear;
    case EasingType::EaseInQuadratic:     return math::easeInQuadratic;
    case EasingType::EaseOutQuadratic:    return math::easeOutQuadratic;
    case EasingType::EaseInOutQuadratic:  return math::easeInOutQuadratic;
    case EasingType::EaseInCubic:         return math::easeInCubic;
    case EasingType::EaseOutCubic:        return math::easeOutCubic;
    case EasingType::EaseInOutCubic:      return math::easeInOutCubic;
    case EasingType::EaseInQuartic:       return math::easeInQuartic;
    case EasingType::EaseOutQuartic:      return math::easeOutQuartic;
    case EasingType::EaseInOutQuartic:    return math::easeInOutQuartic;
    case EasingType::EaseInQuintic:       return math::easeInQuintic;
    case EasingType::EaseOutQuintic:      return math::easeOutQuintic;
    case EasingType::EaseInOutQuintic:    return math::easeInOutQuintic;
    case EasingType::EaseInSin:           return math::easeInSin;
    case EasingType::EaseOutSin:          return math::easeOutSin;
    case EasingType::EaseInOutSin:        return math::easeInOutSin;
    case EasingType::EaseInExponential:   return math::easeInExponential;
    case EasingType::EaseOutExponential:  return math::easeOutExponential;
    case EasingType::EaseInOutExponential:return math::easeInOutExponential;
    case EasingType::EaseInCircular:      return math::easeInCircular;
    case EasingType::EaseOutCircular:     return math::easeOutCircular;
    case EasingType::EaseInOutCircular:   return math::easeInOutCircular;
    case EasingType::EaseInBack:          return math::easeInBack;
    case EasingType::EaseOutBack:         return math::easeOutBack;
    case EasingType::EaseInOutBack:       return math::easeInOutBack;
    case EasingType::EaseInElastic:       return math::easeInElastic;
    case EasingType::EaseOutElastic:      return math::easeOutElastic;
    case EasingType::EaseInOutElastic:    return math::easeInOutElastic;
    case EasingType::EaseInBounce:        return math::easeInBounce;
    case EasingType::EaseOutBounce:       return math::easeOutBounce;
    case EasingType::EaseInOutBounce:     return math::easeInOutBounce;
    default:                              return math::easeInLinear;
    }
}

// Applies an easing function to an interpolation factor t in [0,1].
[[nodiscard]] inline f32 applyEasing(EasingType type, f32 t) noexcept {
    if (type == EasingType::Linear) { return t; }
    return toFunction(type)(t);
}

} // namespace draco::animation
