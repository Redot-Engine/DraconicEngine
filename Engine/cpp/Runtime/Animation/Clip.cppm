/// Animation clip, tracks, and keyframes (`:clip` partition).
///
/// Animation data: keyframed tracks (position/rotation/scale per bone) + timed events, grouped into
/// an AnimationClip. / AnimationTrack /
/// Keyframe / AnimationEvent. Tracks are heap-owned (UniquePtr) so GetOrCreate*Track pointers stay
/// valid as more tracks are added (matching the Beef List<AnimationTrack<T>> of references).

module;
#include <algorithm>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <functional>

export module animation:clip;

import core;

using namespace draco;

export namespace draco::animation {

// Event callback: (event name, event time in seconds). The engine's move-only delegate. Defined here
// (with AnimationEvent) so both the player and the graph's IAnimationStateNode can use it.
using AnimationEventHandler = std::function<void(std::u8string_view, f32)>;

// Keyframe interpolation mode.
enum class InterpolationMode { Step, Linear, CubicSpline };

// A keyframed value at a time (tangents used only for cubic spline).
template <typename T>
struct Keyframe {
    f32 time      = 0.0f;
    T   value     = {};
    T   inTangent = {};
    T   outTangent= {};

    Keyframe() = default;
    Keyframe(f32 t, const T& v) : time(t), value(v) {}
    Keyframe(f32 t, const T& v, const T& in, const T& out) : time(t), value(v), inTangent(in), outTangent(out) {}
};

// The keyframe interval surrounding a sample time + the interpolation factor.
struct KeyframeLookup { i32 prev = -1; i32 next = -1; f32 t = 0.0f; };

// Keyframes for one property of one bone.
template <typename T>
class AnimationTrack {
public:
    i32               boneIndex     = 0;
    InterpolationMode interpolation = InterpolationMode::Linear;

    [[nodiscard]] std::vector<Keyframe<T>>&       keyframes() noexcept { return m_keyframes; }
    [[nodiscard]] const std::vector<Keyframe<T>>& keyframes() const noexcept { return m_keyframes; }

    void addKeyframe(f32 time, const T& value) { m_keyframes.push_back(Keyframe<T>{ time, value }); }
    void addKeyframe(f32 time, const T& value, const T& inTan, const T& outTan) {
        m_keyframes.push_back(Keyframe<T>{ time, value, inTan, outTan });
    }

    // Stable insertion sort by time (keyframes are typically already time-ordered from import).
    void sortKeyframes() {
        for (usize i = 1; i < m_keyframes.size(); ++i) {
            Keyframe<T> key = m_keyframes[i];
            usize j = i;
            while (j > 0 && m_keyframes[j - 1].time > key.time) { m_keyframes[j] = m_keyframes[j - 1]; --j; }
            m_keyframes[j] = key;
        }
    }

    // Find the keyframe interval (prev,next) + factor t for `time`. Clamps to ends.
    [[nodiscard]] KeyframeLookup findKeyframes(f32 time) const {
        const i32 count = static_cast<i32>(m_keyframes.size());
        if (count == 0) { return KeyframeLookup{ -1, -1, 0.0f }; }
        if (count == 1) { return KeyframeLookup{ 0, 0, 0.0f }; }
        if (time <= m_keyframes[0].time) { return KeyframeLookup{ 0, 0, 0.0f }; }
        if (time >= m_keyframes[static_cast<usize>(count - 1)].time) { return KeyframeLookup{ count - 1, count - 1, 0.0f }; }

        i32 low = 0, high = count - 1;
        while (low < high - 1) {
            const i32 mid = (low + high) / 2;
            if (m_keyframes[static_cast<usize>(mid)].time <= time) { low = mid; } else { high = mid; }
        }
        const f32 duration = m_keyframes[static_cast<usize>(high)].time - m_keyframes[static_cast<usize>(low)].time;
        const f32 t = (duration > 0.0f) ? (time - m_keyframes[static_cast<usize>(low)].time) / duration : 0.0f;
        return KeyframeLookup{ low, high, t };
    }

private:
    std::vector<Keyframe<T>> m_keyframes;
};

// An event placed at a time in a clip; fires when playback crosses it.
struct AnimationEvent {
    f32    time = 0.0f;
    std::u8string name;
    AnimationEvent() = default;
    AnimationEvent(f32 t, std::u8string_view n) : time(t), name(n) {}
};

// All tracks + events for one animation. A resource product (Object) so a cooked AnimationClipSource
// can build into it via the resource system.
class AnimationClip {
public:
    AnimationClip() = default;
    explicit AnimationClip(std::u8string_view name, f32 duration = 0.0f, bool isLooping = false)
        : duration(duration), isLooping(isLooping), m_name(name) {}

    AnimationClip(const AnimationClip&) = delete;
    AnimationClip& operator=(const AnimationClip&) = delete;

    f32  duration  = 0.0f;
    bool isLooping = false;

    [[nodiscard]] std::u8string&       name() noexcept { return m_name; }
    [[nodiscard]] const std::u8string& name() const noexcept { return m_name; }

    using Vec3Track = AnimationTrack<math::Vector3>;
    using QuatTrack = AnimationTrack<math::Quaternion>;

    [[nodiscard]] std::vector<std::unique_ptr<Vec3Track>>& positionTracks() noexcept { return m_positionTracks; }
    [[nodiscard]] std::vector<std::unique_ptr<QuatTrack>>& rotationTracks() noexcept { return m_rotationTracks; }
    [[nodiscard]] std::vector<std::unique_ptr<Vec3Track>>& scaleTracks()    noexcept { return m_scaleTracks; }
    [[nodiscard]] std::vector<AnimationEvent>&       events()         noexcept { return m_events; }
    [[nodiscard]] const std::vector<std::unique_ptr<Vec3Track>>& positionTracks() const noexcept { return m_positionTracks; }
    [[nodiscard]] const std::vector<std::unique_ptr<QuatTrack>>& rotationTracks() const noexcept { return m_rotationTracks; }
    [[nodiscard]] const std::vector<std::unique_ptr<Vec3Track>>& scaleTracks()    const noexcept { return m_scaleTracks; }
    [[nodiscard]] const std::vector<AnimationEvent>&       events()         const noexcept { return m_events; }

    [[nodiscard]] Vec3Track* getOrCreatePositionTrack(i32 boneIndex) { return getOrCreate(m_positionTracks, boneIndex); }
    [[nodiscard]] QuatTrack* getOrCreateRotationTrack(i32 boneIndex) { return getOrCreate(m_rotationTracks, boneIndex); }
    [[nodiscard]] Vec3Track* getOrCreateScaleTrack(i32 boneIndex)    { return getOrCreate(m_scaleTracks, boneIndex); }

    void sortAllKeyframes() {
        for (auto& t : m_positionTracks) { t->sortKeyframes(); }
        for (auto& t : m_rotationTracks) { t->sortKeyframes(); }
        for (auto& t : m_scaleTracks)    { t->sortKeyframes(); }
    }

    void addEvent(f32 time, std::u8string_view name) { m_events.push_back(AnimationEvent{ time, name }); }

    void sortEvents() {
        for (usize i = 1; i < m_events.size(); ++i) {
            AnimationEvent key = static_cast<AnimationEvent&&>(m_events[i]);
            usize j = i;
            while (j > 0 && m_events[j - 1].time > key.time) { m_events[j] = static_cast<AnimationEvent&&>(m_events[j - 1]); --j; }
            m_events[j] = static_cast<AnimationEvent&&>(key);
        }
    }

    // Fire events crossed in (prevTime, currentTime]. currentTime may exceed Duration when looping.
    // `handler` is any callable (std::u8string_view name, f32 time).
    template <typename Handler>
    void fireEvents(f32 prevTime, f32 currentTime, Handler&& handler) const {
        if (m_events.empty() || duration <= 0.0f) { return; }

        if (currentTime > prevTime && currentTime <= duration) {
            for (const AnimationEvent& e : m_events) {
                if (e.time > prevTime && e.time <= currentTime) { handler(std::u8string_view{ e.name }, e.time); }
            }
        } else if (currentTime > duration) {
            if (isLooping) {
                for (const AnimationEvent& e : m_events) {
                    if (e.time > prevTime && e.time <= duration) { handler(std::u8string_view{ e.name }, e.time); }
                }
                f32 wrapped = currentTime;
                while (wrapped >= duration) { wrapped -= duration; }
                for (const AnimationEvent& e : m_events) {
                    if (e.time <= wrapped) { handler(std::u8string_view{ e.name }, e.time); }
                }
            } else {
                for (const AnimationEvent& e : m_events) {
                    if (e.time > prevTime && e.time <= duration) { handler(std::u8string_view{ e.name }, e.time); }
                }
            }
        }
    }

    // Duration = latest keyframe time across all tracks.
    void computeDuration() {
        duration = 0.0f;
        for (auto& t : m_positionTracks) { if (!t->keyframes().empty()) { duration = std::max(duration, t->keyframes()[t->keyframes().size() - 1].time); } }
        for (auto& t : m_rotationTracks) { if (!t->keyframes().empty()) { duration = std::max(duration, t->keyframes()[t->keyframes().size() - 1].time); } }
        for (auto& t : m_scaleTracks)    { if (!t->keyframes().empty()) { duration = std::max(duration, t->keyframes()[t->keyframes().size() - 1].time); } }
    }

    // Reset in place (resource hot-reload keeps outside references valid).
    void clearForReload() {
        m_positionTracks.clear(); m_rotationTracks.clear(); m_scaleTracks.clear(); m_events.clear();
        duration = 0.0f; isLooping = false; m_name.clear();
    }

private:
    template <typename Track>
    [[nodiscard]] Track* getOrCreate(std::vector<std::unique_ptr<Track>>& tracks, i32 boneIndex) {
        for (auto& t : tracks) { if (t->boneIndex == boneIndex) { return t.get(); } }
        std::unique_ptr<Track> track = std::make_unique<Track>();
        track->boneIndex = boneIndex;
        Track* raw = track.get();
        tracks.push_back(static_cast<std::unique_ptr<Track>&&>(track));
        return raw;
    }

    std::u8string                     m_name;
    std::vector<std::unique_ptr<Vec3Track>> m_positionTracks;
    std::vector<std::unique_ptr<QuatTrack>> m_rotationTracks;
    std::vector<std::unique_ptr<Vec3Track>> m_scaleTracks;
    std::vector<AnimationEvent>       m_events;
};


} // namespace draco::animation
