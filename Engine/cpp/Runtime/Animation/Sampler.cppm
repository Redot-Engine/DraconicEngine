/// Clip sampling and pose blending (`:sampler` partition).
///
/// Stateless sampling of clips/tracks into bone poses + pose blending (lerp + additive). Ported
/// faithfully from 

module;
#include <cmath>
#include <algorithm>
#include <span>

export module animation:sampler;

import core;
import :skeleton;   // BoneTransform, Skeleton
import :clip;       // AnimationTrack, AnimationClip, InterpolationMode

using namespace draco;

export namespace draco::animation {

// --- cubic spline (Hermite) helpers ---
[[nodiscard]] inline math::Vector3 cubicSplineVec3(const Keyframe<math::Vector3>& prev, const Keyframe<math::Vector3>& next, f32 t, f32 duration) {
    const f32 t2 = t * t, t3 = t2 * t;
    const math::Vector3 p0 = prev.value;
    const math::Vector3 m0 = prev.outTangent * duration;
    const math::Vector3 p1 = next.value;
    const math::Vector3 m1 = next.inTangent * duration;
    const f32 h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const f32 h10 = t3 - 2.0f * t2 + t;
    const f32 h01 = -2.0f * t3 + 3.0f * t2;
    const f32 h11 = t3 - t2;
    return p0 * h00 + m0 * h10 + p1 * h01 + m1 * h11;
}

[[nodiscard]] inline math::Quaternion cubicSplineQuat(const Keyframe<math::Quaternion>& prev, const Keyframe<math::Quaternion>& next, f32 t, f32 /*duration*/) {
    // Simplified Hermite + normalize (squad would be more accurate).
    const f32 t2 = t * t, t3 = t2 * t;
    const f32 h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const f32 h01 = -2.0f * t3 + 3.0f * t2;
    return normalize(slerp(prev.value, next.value, h01 / (h00 + h01)));
}

// Sample a math::Vector3 track at `time` (returns `defaultValue` if empty).
[[nodiscard]] inline math::Vector3 sampleVec3(const AnimationTrack<math::Vector3>* track, f32 time, math::Vector3 defaultValue = math::Vector3{ 0, 0, 0 }) {
    if (track == nullptr || track->keyframes().empty()) { return defaultValue; }
    const KeyframeLookup k = track->findKeyframes(time);
    if (k.prev < 0) { return defaultValue; }
    const Keyframe<math::Vector3>& prev = track->keyframes()[static_cast<usize>(k.prev)];
    const Keyframe<math::Vector3>& next = track->keyframes()[static_cast<usize>(k.next)];
    switch (track->interpolation) {
    case InterpolationMode::Step:   return prev.value;
    case InterpolationMode::Linear: return lerp(prev.value, next.value, k.t);
    case InterpolationMode::CubicSpline: return cubicSplineVec3(prev, next, k.t, next.time - prev.time);
    }
    return prev.value;
}

// Sample a math::Quaternion track at `time` (returns `defaultValue` if empty).
[[nodiscard]] inline math::Quaternion sampleQuat(const AnimationTrack<math::Quaternion>* track, f32 time, math::Quaternion defaultValue = math::Quaternion::identity) {
    if (track == nullptr || track->keyframes().empty()) { return defaultValue; }
    const KeyframeLookup k = track->findKeyframes(time);
    if (k.prev < 0) { return defaultValue; }
    const Keyframe<math::Quaternion>& prev = track->keyframes()[static_cast<usize>(k.prev)];
    const Keyframe<math::Quaternion>& next = track->keyframes()[static_cast<usize>(k.next)];
    switch (track->interpolation) {
    case InterpolationMode::Step:   return prev.value;
    case InterpolationMode::Linear: return slerp(prev.value, next.value, k.t);
    case InterpolationMode::CubicSpline: return cubicSplineQuat(prev, next, k.t, next.time - prev.time);
    }
    return prev.value;
}

// Sample a clip at `time` into `outPoses` (bind pose for un-animated bones). Handles looping/clamp.
inline void sampleClip(const AnimationClip& clip, const Skeleton& skeleton, f32 time, std::span<BoneTransform> outPoses) {
    const i32 boneCount = skeleton.boneCount();
    for (i32 i = 0; i < boneCount && static_cast<usize>(i) < outPoses.size(); ++i) {
        const Bone* bone = skeleton.getBone(i);
        outPoses[static_cast<usize>(i)] = (bone != nullptr) ? bone->localBindPose : BoneTransform{};
    }

    f32 sampleTime = time;
    if (clip.isLooping && clip.duration > 0.0f) {
        sampleTime = time - clip.duration * std::floor(time / clip.duration);   // positive modulo
        if (sampleTime < 0.0f) { sampleTime += clip.duration; }
    } else {
        sampleTime = std::clamp(time, 0.0f, clip.duration);
    }

    for (const auto& track : clip.positionTracks()) {
        const i32 b = track->boneIndex;
        if (b >= 0 && static_cast<usize>(b) < outPoses.size()) {
            outPoses[static_cast<usize>(b)].position = sampleVec3(track.get(), sampleTime, outPoses[static_cast<usize>(b)].position);
        }
    }
    for (const auto& track : clip.rotationTracks()) {
        const i32 b = track->boneIndex;
        if (b >= 0 && static_cast<usize>(b) < outPoses.size()) {
            outPoses[static_cast<usize>(b)].rotation = sampleQuat(track.get(), sampleTime, outPoses[static_cast<usize>(b)].rotation);
        }
    }
    for (const auto& track : clip.scaleTracks()) {
        const i32 b = track->boneIndex;
        if (b >= 0 && static_cast<usize>(b) < outPoses.size()) {
            outPoses[static_cast<usize>(b)].scale = sampleVec3(track.get(), sampleTime, outPoses[static_cast<usize>(b)].scale);
        }
    }
}

// Linear blend of two poses (0 = a, 1 = b).
inline void blendPoses(std::span<const BoneTransform> a, std::span<const BoneTransform> b, f32 factor, std::span<BoneTransform> out) {
    const usize count = std::min(std::min(a.size(), b.size()), out.size());
    for (usize i = 0; i < count; ++i) { out[i] = BoneTransform::lerp(a[i], b[i], factor); }
}

// Additive blend: base + (additive * weight).
inline void additivePoses(std::span<const BoneTransform> base, std::span<const BoneTransform> additive, f32 weight, std::span<BoneTransform> out) {
    const usize count = std::min(std::min(base.size(), additive.size()), out.size());
    for (usize i = 0; i < count; ++i) {
        out[i].position = base[i].position + additive[i].position * weight;
        out[i].rotation = slerp(math::Quaternion::identity, additive[i].rotation, weight) * base[i].rotation;
        out[i].scale    = base[i].scale * lerp(math::Vector3::one, additive[i].scale, weight);
    }
}

} // namespace draco::animation
