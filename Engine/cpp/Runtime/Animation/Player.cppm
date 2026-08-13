/// Single-clip animation player (`:player` partition).
///
/// AnimationPlayer: single-clip playback for one skeleton instance - time advance, looping/clamping,
/// event firing, and evaluation into skinning matrices (+ previous frame for motion vectors). Ported
/// faithfully from  The skeleton + clip are borrowed (not owned).

module;
#include <algorithm>
#include <vector>
#include <span>
#include <string_view>

export module animation:player;

import core;
import :skeleton;
import :clip;
import :sampler;
import :pose;

using namespace draco;

export namespace draco::animation {

enum class PlaybackState { Stopped, Playing, Paused };
// AnimationEventHandler is defined in :clip (shared with the graph).

class AnimationPlayer {
public:
    explicit AnimationPlayer(Skeleton& skeleton) : m_skeleton(&skeleton) {
        const usize boneCount = static_cast<usize>(skeleton.boneCount());
        m_localPoses.resize(boneCount);
        m_skinningMatrices.resize(boneCount);
        m_prevSkinningMatrices.resize(boneCount);
        resetToBind();
    }

    ~AnimationPlayer() = default;
    AnimationPlayer(const AnimationPlayer&) = delete;
    AnimationPlayer& operator=(const AnimationPlayer&) = delete;

    [[nodiscard]] Skeleton&       getSkeleton() noexcept { return *m_skeleton; }
    [[nodiscard]] AnimationClip*  currentClip() const noexcept { return m_currentClip; }
    [[nodiscard]] PlaybackState   state() const noexcept { return m_state; }
    [[nodiscard]] f32             currentTime() const noexcept { return m_currentTime; }

    f32 speed = 1.0f;   // playback speed multiplier

    // Setting the time marks the skinning matrices dirty (re-evaluated on next access).
    void setCurrentTime(f32 t) noexcept { if (m_currentTime != t) { m_currentTime = t; m_matricesDirty = true; } }

    // Plays a clip (borrowed). restart resets the clock to 0.
    void play(AnimationClip* clip, bool restart = true) {
        m_currentClip = clip;
        if (restart) { m_currentTime = 0.0f; m_prevTime = 0.0f; }
        m_state = PlaybackState::Playing;
        m_matricesDirty = true;
    }

    void stop() {
        m_state = PlaybackState::Stopped;
        m_currentTime = 0.0f; m_prevTime = 0.0f;
        m_currentClip = nullptr;
        resetToBind();
    }

    void pause()  { if (m_state == PlaybackState::Playing) { m_state = PlaybackState::Paused; } }
    void resume() { if (m_state == PlaybackState::Paused)  { m_state = PlaybackState::Playing; } }

    // Sets the event handler (takes ownership; replaces any previous handler).
    void setEventHandler(AnimationEventHandler handler) { m_eventHandler = static_cast<AnimationEventHandler&&>(handler); }

    // Resets all local poses to the skeleton's bind pose.
    void resetToBind() {
        for (i32 i = 0; i < m_skeleton->boneCount() && static_cast<usize>(i) < m_localPoses.size(); ++i) {
            const Bone* bone = m_skeleton->getBone(i);
            m_localPoses[static_cast<usize>(i)] = (bone != nullptr) ? bone->localBindPose : BoneTransform{};
        }
        m_matricesDirty = true;
    }

    // Advances playback by deltaTime: fires crossed events, then loops/clamps.
    void update(f32 deltaTime) {
        if (m_state != PlaybackState::Playing || m_currentClip == nullptr) { return; }

        // Snapshot current skinning matrices as the previous frame (motion vectors).
        const usize n = m_skinningMatrices.size();
        for (usize i = 0; i < n; ++i) { m_prevSkinningMatrices[i] = m_skinningMatrices[i]; }

        const f32 prevTime = m_prevTime;
        m_currentTime += deltaTime * speed;

        // Fire events before wrapping (so loop crossings are detected).
        if (m_eventHandler && !m_currentClip->events().empty()) {
            m_currentClip->fireEvents(prevTime, m_currentTime,
                                      [this](std::u8string_view name, f32 time) { m_eventHandler(name, time); });
        }

        if (m_currentClip->isLooping) {
            if (m_currentClip->duration > 0.0f) {
                while (m_currentTime >= m_currentClip->duration) { m_currentTime -= m_currentClip->duration; }
                while (m_currentTime < 0.0f) { m_currentTime += m_currentClip->duration; }
            }
        } else {
            if (m_currentTime >= m_currentClip->duration) { m_currentTime = m_currentClip->duration; m_state = PlaybackState::Stopped; }
            else if (m_currentTime < 0.0f) { m_currentTime = 0.0f; m_state = PlaybackState::Stopped; }
        }

        m_prevTime = m_currentTime;
        m_matricesDirty = true;
    }

    // Samples the current clip + computes skinning matrices (only if dirty). Call before rendering.
    void evaluate() {
        if (!m_matricesDirty) { return; }
        if (m_currentClip != nullptr) {
            sampleClip(*m_currentClip, *m_skeleton, m_currentTime, std::span<BoneTransform>{ m_localPoses.data(), m_localPoses.size() });
        }
        m_skeleton->computeSkinningMatrices(std::span<const BoneTransform>{ m_localPoses.data(), m_localPoses.size() },
                                            std::span<math::Matrix4>{ m_skinningMatrices.data(), m_skinningMatrices.size() });
        m_matricesDirty = false;
    }

    // Current skinning matrices for GPU upload (evaluates if needed).
    [[nodiscard]] std::span<const math::Matrix4> getSkinningMatrices() {
        evaluate();
        return std::span<const math::Matrix4>{ m_skinningMatrices.data(), m_skinningMatrices.size() };
    }
    [[nodiscard]] std::span<const math::Matrix4> getPrevSkinningMatrices() const {
        return std::span<const math::Matrix4>{ m_prevSkinningMatrices.data(), m_prevSkinningMatrices.size() };
    }

    // Push externally-computed matrices (used by the graph player to drive this player's output).
    void overrideSkinningMatrices(std::span<const math::Matrix4> current, std::span<const math::Matrix4> prev) {
        const usize c = std::min(current.size(), m_skinningMatrices.size());
        for (usize i = 0; i < c; ++i) { m_skinningMatrices[i] = current[i]; }
        const usize p = std::min(prev.size(), m_prevSkinningMatrices.size());
        for (usize i = 0; i < p; ++i) { m_prevSkinningMatrices[i] = prev[i]; }
        m_matricesDirty = false;
    }

    [[nodiscard]] std::span<BoneTransform> getLocalPoses() noexcept { return { m_localPoses.data(), m_localPoses.size() }; }
    [[nodiscard]] AnimationPose       getPose() noexcept { return AnimationPose{ getLocalPoses() }; }

    // Directly set a bone's local transform (procedural animation).
    void setBonePose(i32 boneIndex, const BoneTransform& pose) {
        if (boneIndex >= 0 && static_cast<usize>(boneIndex) < m_localPoses.size()) {
            m_localPoses[static_cast<usize>(boneIndex)] = pose;
            m_matricesDirty = true;
        }
    }

    // Blend another clip on top of the current local poses (weight 0..1).
    void blendAnimation(AnimationClip* clip, f32 time, f32 weight) {
        if (clip == nullptr || weight <= 0.0f) { return; }
        std::vector<BoneTransform> blend; blend.resize(static_cast<usize>(m_skeleton->boneCount()));
        sampleClip(*clip, *m_skeleton, time, std::span<BoneTransform>{ blend.data(), blend.size() });
        const usize n = std::min(m_localPoses.size(), blend.size());
        for (usize i = 0; i < n; ++i) { m_localPoses[i] = BoneTransform::lerp(m_localPoses[i], blend[i], weight); }
        m_matricesDirty = true;
    }

private:
    Skeleton*      m_skeleton    = nullptr;   // borrowed
    AnimationClip* m_currentClip = nullptr;   // borrowed
    f32            m_currentTime = 0.0f;
    f32            m_prevTime    = 0.0f;
    PlaybackState  m_state       = PlaybackState::Stopped;
    bool           m_matricesDirty = true;

    std::vector<BoneTransform> m_localPoses;
    std::vector<math::Matrix4>          m_skinningMatrices;
    std::vector<math::Matrix4>          m_prevSkinningMatrices;
    AnimationEventHandler m_eventHandler;
};

} // namespace draco::animation
