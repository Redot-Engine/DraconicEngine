/// Animation pose (`:pose` partition).
///
/// AnimationPose: a non-owning view over per-bone local transforms (+ optional morph weights)
/// representing one evaluated pose. The backing arrays must outlive the view. Ported faithfully from
/// 

module;
#include <span>

export module animation:pose;

import core;
import :skeleton;   // BoneTransform

using namespace draco;

export namespace draco::animation {

struct AnimationPose {
    std::span<BoneTransform> boneTransforms;   // per-bone local transforms
    std::span<f32>           morphWeights;     // per-morph-target weights (empty until morph support)

    AnimationPose() = default;
    explicit AnimationPose(std::span<BoneTransform> bones, std::span<f32> morphs = {}) noexcept
        : boneTransforms(bones), morphWeights(morphs) {}

    [[nodiscard]] usize boneCount() const noexcept { return boneTransforms.size(); }
    [[nodiscard]] bool  hasMorphWeights() const noexcept { return morphWeights.size() > 0; }
};

} // namespace draco::animation
