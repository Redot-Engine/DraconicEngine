/// Animation types: interpolation, channels, keyframes, and animation clips.

module;
#include <span>
#include <string_view>

#include <string>
#include <vector>

export module model:model_animation;

import core;

using namespace draco;

export namespace draco::model {

/// Animation interpolation type.
enum class AnimationInterpolation : u32 {
    Linear,
    Step,
    CubicSpline,
};

/// Animation channel target path.
enum class AnimationPath : u32 {
    Translation,
    Rotation,
    Scale,
    Weights, // Morph target weights.
};

/// Keyframe data for an animation.
struct AnimationKeyframe {
    f32  time  = 0.0f;
    math::Vector4 value{}; // Translation (xyz), Rotation (xyzw), Scale (xyz), or single Weight.

    constexpr AnimationKeyframe() = default;
    constexpr AnimationKeyframe(f32 t, math::Vector4 v) : time(t), value(v) {}
};

/// An animation channel targeting a specific bone/node property.
class AnimationChannel {
public:
    AnimationChannel() = default;
    ~AnimationChannel() = default;

    /// Add a keyframe.
    void addKeyframe(f32 time, math::Vector4 value) {
        m_keyframes.push_back(AnimationKeyframe(time, value));
    }

    [[nodiscard]] std::span<const AnimationKeyframe> keyframes() const {
        return std::span<const AnimationKeyframe>(m_keyframes.data(), m_keyframes.size());
    }
    [[nodiscard]] std::span<AnimationKeyframe> keyframes() {
        return std::span<AnimationKeyframe>(m_keyframes.data(), m_keyframes.size());
    }

    /// Sample the animation at a given time.
    [[nodiscard]] math::Vector4 sample(f32 time) const {
        if (m_keyframes.empty()) return {};
        if (m_keyframes.size() == 1) return m_keyframes[0].value;

        // Clamp to animation bounds.
        if (time <= m_keyframes[0].time)
            return m_keyframes[0].value;
        if (time >= m_keyframes.back().time)
            return m_keyframes.back().value;

        // Find surrounding keyframes.
        usize i = 0;
        while (i < m_keyframes.size() - 1 && m_keyframes[i + 1].time < time)
            ++i;

        auto& k0 = m_keyframes[i];
        auto& k1 = m_keyframes[i + 1];

        f32 t = (time - k0.time) / (k1.time - k0.time);

        switch (interpolation) {
        case AnimationInterpolation::Step:
            return k0.value;
        case AnimationInterpolation::Linear:
            if (path == AnimationPath::Rotation) {
                // math::Quaternion slerp.
                math::Quaternion q0(k0.value.x, k0.value.y, k0.value.z, k0.value.w);
                math::Quaternion q1(k1.value.x, k1.value.y, k1.value.z, k1.value.w);
                math::Quaternion result = slerp(q0, q1, t);
                return math::Vector4(result.x, result.y, result.z, result.w);
            } else {
                return lerp(k0.value, k1.value, t);
            }
        case AnimationInterpolation::CubicSpline:
            // TODO: Implement cubic spline interpolation.
            return lerp(k0.value, k1.value, t);
        }
        return {};
    }

    // -- Public fields --

    /// Target bone index.
    i32 targetBone = 0;

    /// Property being animated.
    AnimationPath path = AnimationPath::Translation;

    /// Interpolation method.
    AnimationInterpolation interpolation = AnimationInterpolation::Linear;

private:
    std::vector<AnimationKeyframe> m_keyframes;
};

/// A complete animation clip.
class ModelAnimation {
public:
    ModelAnimation() = default;

    ~ModelAnimation() {
        for (auto* ch : m_channels)
            delete ch;
    }

    // Non-copyable, movable.
    ModelAnimation(const ModelAnimation&) = delete;
    ModelAnimation& operator=(const ModelAnimation&) = delete;
    ModelAnimation(ModelAnimation&& other) noexcept
        : duration(other.duration),
          m_name(static_cast<std::u8string&&>(other.m_name)),
          m_channels(static_cast<std::vector<AnimationChannel*>&&>(other.m_channels)) {
        other.m_channels.clear();
    }
    ModelAnimation& operator=(ModelAnimation&& other) noexcept {
        if (this != &other) {
            for (auto* ch : m_channels) delete ch;
            m_name = static_cast<std::u8string&&>(other.m_name);
            m_channels = static_cast<std::vector<AnimationChannel*>&&>(other.m_channels);
            duration = other.duration;
            other.m_channels.clear();
        }
        return *this;
    }

    [[nodiscard]] std::u8string_view name() const { return std::u8string_view(m_name.data(), m_name.size()); }
    void setName(std::u8string_view n) { m_name = std::u8string(n); }

    /// Add a channel (takes ownership of raw pointer).
    void addChannel(AnimationChannel* channel) {
        m_channels.push_back(channel);
    }

    [[nodiscard]] std::span<AnimationChannel* const> channels() const {
        return std::span<AnimationChannel* const>(m_channels.data(), m_channels.size());
    }
    [[nodiscard]] std::span<AnimationChannel*> channels() {
        return std::span<AnimationChannel*>(m_channels.data(), m_channels.size());
    }

    /// Calculate duration from keyframes.
    void calculateDuration() {
        duration = 0.0f;
        for (auto* channel : m_channels) {
            for (auto& kf : channel->keyframes()) {
                if (kf.time > duration)
                    duration = kf.time;
            }
        }
    }

    // -- Public fields --

    /// Duration of the animation in seconds.
    f32 duration = 0.0f;

private:
    std::u8string m_name;
    std::vector<AnimationChannel*> m_channels;
};

} // namespace draco::model
