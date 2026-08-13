/// Animation state graph and blend trees (`:graph` partition).
///
/// The animation graph stack: state nodes (clip + 1D/2D
/// blend trees), per-bone masks, parameters + conditions + transitions, states, layers, the graph
/// definition, and the per-instance AnimationGraphPlayer (state machine + layer blending). Resource
/// refs (editor/serialization) are dropped - this is the runtime foundation. Polymorphic node
/// dispatch uses a NodeType tag + static_cast (the engine builds with -fno-rtti, so no dynamic_cast).

module;
#include <cmath>
#include <algorithm>
#include <vector>
#include <span>
#include <string>
#include <string_view>
#include <memory>

export module animation:graph;

import core;
import :skeleton;   // Skeleton, Bone, BoneTransform
import :clip;       // AnimationClip, AnimationEventHandler
import :sampler;    // SampleClip, BlendPoses

using namespace draco;

export namespace draco::animation {

// ---- state nodes ---------------------------------------------------------------------------

enum class NodeType { Clip, BlendTree1D, BlendTree2D };

// A node that produces an animation pose. Implemented by ClipStateNode + the blend trees.
class IAnimationStateNode {
public:
    virtual ~IAnimationStateNode() = default;
    [[nodiscard]] virtual NodeType type() const noexcept = 0;
    // Evaluate at normalized time [0..1] into outPoses.
    virtual void evaluate(const Skeleton& skeleton, f32 normalizedTime, std::span<BoneTransform> outPoses) const = 0;
    [[nodiscard]] virtual f32 duration() const noexcept = 0;
    virtual void fireEvents(f32 prevNormalizedTime, f32 currentNormalizedTime, bool looping, const AnimationEventHandler& handler) const = 0;
};

// Wraps a single AnimationClip (borrowed).
class ClipStateNode final : public IAnimationStateNode {
public:
    explicit ClipStateNode(AnimationClip* clip) : m_clip(clip) {}
    [[nodiscard]] NodeType type() const noexcept override { return NodeType::Clip; }
    [[nodiscard]] AnimationClip* clip() const noexcept { return m_clip; }

    void evaluate(const Skeleton& skeleton, f32 normalizedTime, std::span<BoneTransform> outPoses) const override {
        if (m_clip == nullptr) { return; }
        sampleClip(*m_clip, skeleton, normalizedTime * m_clip->duration, outPoses);
    }
    [[nodiscard]] f32 duration() const noexcept override { return m_clip != nullptr ? m_clip->duration : 0.0f; }

    void fireEvents(f32 prevNorm, f32 currentNorm, bool looping, const AnimationEventHandler& handler) const override {
        if (m_clip == nullptr || !handler || m_clip->events().empty() || m_clip->duration <= 0.0f) { return; }
        const f32 prevAbs = prevNorm * m_clip->duration;
        const f32 curAbs  = currentNorm * m_clip->duration;
        if (looping && currentNorm < prevNorm) {
            // wrapped: (prevAbs, Duration] then [0, curAbs]
            for (const AnimationEvent& e : m_clip->events()) { if (e.time > prevAbs && e.time <= m_clip->duration) { handler(std::u8string_view{ e.name }, e.time); } }
            for (const AnimationEvent& e : m_clip->events()) { if (e.time <= curAbs) { handler(std::u8string_view{ e.name }, e.time); } }
        } else {
            m_clip->fireEvents(prevAbs, curAbs, handler);
        }
    }
private:
    AnimationClip* m_clip = nullptr;
};

// ---- bone mask -----------------------------------------------------------------------------

class BoneMask {
public:
    explicit BoneMask(i32 boneCount, f32 defaultWeight = 1.0f) {
        m_weights.resize(boneCount < 0 ? 0 : static_cast<usize>(boneCount));
        for (usize i = 0; i < m_weights.size(); ++i) { m_weights[i] = defaultWeight; }
    }
    [[nodiscard]] i32 boneCount() const noexcept { return static_cast<i32>(m_weights.size()); }
    [[nodiscard]] f32 getWeight(i32 boneIndex) const {
        return (boneIndex >= 0 && static_cast<usize>(boneIndex) < m_weights.size()) ? m_weights[static_cast<usize>(boneIndex)] : 0.0f;
    }
    void setWeight(i32 boneIndex, f32 weight) {
        if (boneIndex >= 0 && static_cast<usize>(boneIndex) < m_weights.size()) { m_weights[static_cast<usize>(boneIndex)] = std::clamp(weight, 0.0f, 1.0f); }
    }
    void setAll(f32 weight) { const f32 c = std::clamp(weight, 0.0f, 1.0f); for (usize i = 0; i < m_weights.size(); ++i) { m_weights[i] = c; } }
    void setBoneChainWeight(const Skeleton& skeleton, i32 boneIndex, f32 weight) {
        if (boneIndex < 0 || boneIndex >= skeleton.boneCount()) { return; }
        const f32 c = std::clamp(weight, 0.0f, 1.0f);
        setWeight(boneIndex, c);
        if (const Bone* bone = skeleton.getBone(boneIndex)) {
            for (i32 child : bone->children) { setBoneChainWeight(skeleton, child, c); }
        }
    }
    [[nodiscard]] std::span<const f32> weights() const noexcept { return { m_weights.data(), m_weights.size() }; }
private:
    std::vector<f32> m_weights;
};

// ---- blend trees ---------------------------------------------------------------------------

struct BlendTree1DEntry { f32 threshold = 0.0f; AnimationClip* clip = nullptr; };

// Blends clips along one float axis (Parameter). Entries kept sorted by threshold.
class BlendTree1D final : public IAnimationStateNode {
public:
    i32 parameterIndex = -1;   // graph parameter that drives this tree
    f32 parameter      = 0.0f;

    [[nodiscard]] NodeType type() const noexcept override { return NodeType::BlendTree1D; }
    [[nodiscard]] std::vector<BlendTree1DEntry>& entries() noexcept { return m_entries; }

    void addEntry(f32 threshold, AnimationClip* clip) {
        // Append then bubble left into sorted-by-threshold position (Array has no Insert).
        m_entries.push_back(BlendTree1DEntry{ threshold, clip });
        for (usize i = m_entries.size() - 1; i > 0; --i) {
            if (m_entries[i - 1].threshold <= m_entries[i].threshold) { break; }
            const BlendTree1DEntry tmp = m_entries[i - 1]; m_entries[i - 1] = m_entries[i]; m_entries[i] = tmp;
        }
    }

    void evaluate(const Skeleton& skeleton, f32 normalizedTime, std::span<BoneTransform> outPoses) const override {
        if (m_entries.empty()) { return; }
        if (m_entries.size() == 1) { sampleEntry(0, skeleton, normalizedTime, outPoses); return; }
        if (parameter <= m_entries[0].threshold) { sampleEntry(0, skeleton, normalizedTime, outPoses); return; }
        if (parameter >= m_entries[m_entries.size() - 1].threshold) { sampleEntry(m_entries.size() - 1, skeleton, normalizedTime, outPoses); return; }

        usize lowIdx = 0, highIdx = 1;
        for (usize i = 0; i + 1 < m_entries.size(); ++i) {
            if (parameter >= m_entries[i].threshold && parameter <= m_entries[i + 1].threshold) { lowIdx = i; highIdx = i + 1; break; }
        }
        AnimationClip* clipA = m_entries[lowIdx].clip;
        AnimationClip* clipB = m_entries[highIdx].clip;
        if (clipA == nullptr && clipB == nullptr) { return; }
        if (clipA == nullptr) { sampleEntry(highIdx, skeleton, normalizedTime, outPoses); return; }
        if (clipB == nullptr) { sampleEntry(lowIdx, skeleton, normalizedTime, outPoses); return; }

        const f32 range = m_entries[highIdx].threshold - m_entries[lowIdx].threshold;
        const f32 blend = (range > 0.0f) ? (parameter - m_entries[lowIdx].threshold) / range : 0.0f;
        const usize n = static_cast<usize>(skeleton.boneCount());
        std::vector<BoneTransform> a; a.resize(n);
        std::vector<BoneTransform> b; b.resize(n);
        sampleClip(*clipA, skeleton, normalizedTime * clipA->duration, std::span<BoneTransform>{ a.data(), n });
        sampleClip(*clipB, skeleton, normalizedTime * clipB->duration, std::span<BoneTransform>{ b.data(), n });
        blendPoses(std::span<const BoneTransform>{ a.data(), n }, std::span<const BoneTransform>{ b.data(), n }, blend, outPoses);
    }

    [[nodiscard]] f32 duration() const noexcept override {
        f32 bestDist = 3.4e38f, bestDuration = 0.0f;
        for (const BlendTree1DEntry& e : m_entries) {
            const f32 dist = std::abs(e.threshold - parameter);
            if (dist < bestDist && e.clip != nullptr) { bestDist = dist; bestDuration = e.clip->duration; }
        }
        return bestDuration;
    }
    void fireEvents(f32, f32, bool, const AnimationEventHandler&) const override {}   // blend trees don't fire clip events

private:
    void sampleEntry(usize idx, const Skeleton& skeleton, f32 normalizedTime, std::span<BoneTransform> outPoses) const {
        AnimationClip* clip = m_entries[idx].clip;
        if (clip != nullptr) { sampleClip(*clip, skeleton, normalizedTime * clip->duration, outPoses); }
    }
    std::vector<BlendTree1DEntry> m_entries;
};

struct BlendTree2DEntry { math::Vector2 position = math::Vector2{ 0, 0 }; AnimationClip* clip = nullptr; };

// Blends clips in a 2D parameter space via inverse-distance weighting.
class BlendTree2D final : public IAnimationStateNode {
public:
    i32 parameterIndexX = -1, parameterIndexY = -1;
    f32 parameterX = 0.0f, parameterY = 0.0f;

    [[nodiscard]] NodeType type() const noexcept override { return NodeType::BlendTree2D; }
    [[nodiscard]] std::vector<BlendTree2DEntry>& entries() noexcept { return m_entries; }
    void addEntry(math::Vector2 position, AnimationClip* clip) { m_entries.push_back(BlendTree2DEntry{ position, clip }); }
    void addEntry(f32 x, f32 y, AnimationClip* clip) { m_entries.push_back(BlendTree2DEntry{ math::Vector2{ x, y }, clip }); }

    void evaluate(const Skeleton& skeleton, f32 normalizedTime, std::span<BoneTransform> outPoses) const override {
        if (m_entries.empty()) { return; }
        if (m_entries.size() == 1) { sampleEntry(0, skeleton, normalizedTime, outPoses); return; }

        const math::Vector2 paramPos{ parameterX, parameterY };
        std::vector<f32> weights; weights.resize(m_entries.size());
        f32 totalWeight = 0.0f;
        for (usize i = 0; i < m_entries.size(); ++i) {
            const f32 dist = length(paramPos - m_entries[i].position);
            if (dist < 0.0001f) { sampleEntry(i, skeleton, normalizedTime, outPoses); return; }   // on top of an entry
            weights[i] = 1.0f / dist;
            totalWeight += weights[i];
        }
        if (totalWeight > 0.0f) { for (usize i = 0; i < weights.size(); ++i) { weights[i] /= totalWeight; } }

        const usize n = static_cast<usize>(skeleton.boneCount());
        std::vector<BoneTransform> temp; temp.resize(n);
        bool firstSample = true;
        for (usize i = 0; i < m_entries.size(); ++i) {
            if (weights[i] < 0.001f || m_entries[i].clip == nullptr) { continue; }
            AnimationClip* clip = m_entries[i].clip;
            sampleClip(*clip, skeleton, normalizedTime * clip->duration, std::span<BoneTransform>{ temp.data(), n });
            if (firstSample) {
                for (usize b = 0; b < n && b < outPoses.size(); ++b) { outPoses[b] = temp[b]; }
                firstSample = false;
                if (weights[i] > 0.999f) { return; }
                continue;
            }
            f32 accumulated = 0.0f;
            for (usize j = 0; j < i; ++j) { if (weights[j] >= 0.001f && m_entries[j].clip != nullptr) { accumulated += weights[j]; } }
            const f32 relative = weights[i] / (accumulated + weights[i]);
            blendPoses(std::span<const BoneTransform>{ outPoses.data(), outPoses.size() }, std::span<const BoneTransform>{ temp.data(), n }, relative, outPoses);
        }
    }

    [[nodiscard]] f32 duration() const noexcept override {
        const math::Vector2 paramPos{ parameterX, parameterY };
        f32 totalWeight = 0.0f, totalDuration = 0.0f;
        for (const BlendTree2DEntry& e : m_entries) {
            if (e.clip == nullptr) { continue; }
            const f32 dist = length(paramPos - e.position);
            if (dist < 0.0001f) { return e.clip->duration; }
            const f32 w = 1.0f / dist;
            totalWeight += w; totalDuration += w * e.clip->duration;
        }
        return totalWeight > 0.0f ? totalDuration / totalWeight : 0.0f;
    }
    void fireEvents(f32, f32, bool, const AnimationEventHandler&) const override {}

private:
    void sampleEntry(usize idx, const Skeleton& skeleton, f32 normalizedTime, std::span<BoneTransform> outPoses) const {
        AnimationClip* clip = m_entries[idx].clip;
        if (clip != nullptr) { sampleClip(*clip, skeleton, normalizedTime * clip->duration, outPoses); }
    }
    std::vector<BlendTree2DEntry> m_entries;
};

// ---- parameters + conditions + transitions -------------------------------------------------

enum class AnimationParameterType { Float, Int, Bool, Trigger };

// A named value driving transitions/blend trees. Value type (copied into the player's runtime set).
class AnimationGraphParameter {
public:
    AnimationGraphParameter() = default;
    AnimationGraphParameter(std::u8string_view name, AnimationParameterType t) : type(t), m_name(name) {}

    AnimationParameterType type = AnimationParameterType::Float;
    f32 floatValue = 0.0f;
    i32 intValue   = 0;
    bool boolValue = false;

    [[nodiscard]] const std::u8string& name() const noexcept { return m_name; }
    void consumeTrigger() { if (type == AnimationParameterType::Trigger) { boolValue = false; } }
private:
    std::u8string m_name;
};

enum class ComparisonOp { Equal, NotEqual, Greater, Less, GreaterEqual, LessEqual };

// One transition condition: compare a parameter against a threshold.
struct AnimationGraphCondition {
    i32          parameterIndex = 0;
    ComparisonOp op             = ComparisonOp::Equal;
    f32          threshold      = 0.0f;

    AnimationGraphCondition() = default;
    AnimationGraphCondition(i32 paramIndex, ComparisonOp o, f32 thr = 0.0f) : parameterIndex(paramIndex), op(o), threshold(thr) {}

    [[nodiscard]] bool evaluate(const AnimationGraphParameter* param) const {
        if (param == nullptr) { return false; }
        switch (param->type) {
        case AnimationParameterType::Float: return compareFloat(param->floatValue);
        case AnimationParameterType::Int:   return compareFloat(static_cast<f32>(param->intValue));
        case AnimationParameterType::Bool:
        case AnimationParameterType::Trigger:
            switch (op) {
            case ComparisonOp::Equal:    return param->boolValue == (threshold > 0.5f);
            case ComparisonOp::NotEqual: return param->boolValue != (threshold > 0.5f);
            default:                     return param->boolValue;
            }
        }
        return false;
    }
private:
    [[nodiscard]] bool compareFloat(f32 value) const {
        switch (op) {
        case ComparisonOp::Equal:        return std::abs(value - threshold) < 0.0001f;
        case ComparisonOp::NotEqual:     return std::abs(value - threshold) >= 0.0001f;
        case ComparisonOp::Greater:      return value > threshold;
        case ComparisonOp::Less:         return value < threshold;
        case ComparisonOp::GreaterEqual: return value >= threshold;
        case ComparisonOp::LessEqual:    return value <= threshold;
        }
        return false;
    }
};

// A transition between states; fires when all conditions hold (+ optional exit-time gate).
class AnimationGraphTransition {
public:
    i32  sourceStateIndex = -1;    // -1 = "Any State"
    i32  destStateIndex   = 0;
    f32  duration         = 0.25f; // cross-fade seconds
    bool hasExitTime      = false;
    f32  exitTime         = 1.0f;
    i32  priority         = 0;     // lower = higher priority

    [[nodiscard]] std::vector<AnimationGraphCondition>& conditions() noexcept { return m_conditions; }
    [[nodiscard]] const std::vector<AnimationGraphCondition>& conditions() const noexcept { return m_conditions; }

    void addBoolCondition(i32 parameterIndex, bool expected = true) { m_conditions.push_back(AnimationGraphCondition{ parameterIndex, ComparisonOp::Equal, expected ? 1.0f : 0.0f }); }
    void addFloatCondition(i32 parameterIndex, ComparisonOp op, f32 threshold) { m_conditions.push_back(AnimationGraphCondition{ parameterIndex, op, threshold }); }
    void addIntCondition(i32 parameterIndex, ComparisonOp op, i32 threshold) { m_conditions.push_back(AnimationGraphCondition{ parameterIndex, op, static_cast<f32>(threshold) }); }

    // All conditions must hold against the parameter set. Empty -> unconditional (true).
    [[nodiscard]] bool evaluateConditions(std::span<const AnimationGraphParameter> parameters) const {
        for (const AnimationGraphCondition& c : m_conditions) {
            if (c.parameterIndex < 0 || static_cast<usize>(c.parameterIndex) >= parameters.size()) { return false; }
            if (!c.evaluate(&parameters[static_cast<usize>(c.parameterIndex)])) { return false; }
        }
        return true;
    }
private:
    std::vector<AnimationGraphCondition> m_conditions;
};

// ---- states + layers + graph ---------------------------------------------------------------

// A state wraps a node (clip or blend tree) + playback settings. Owns the node when constructed with
// a UniquePtr; borrows it (no delete) when constructed with a raw pointer.
class AnimationGraphState {
public:
    AnimationGraphState(std::u8string_view name, IAnimationStateNode* node) : m_name(name), m_node(node) {}
    AnimationGraphState(std::u8string_view name, std::unique_ptr<IAnimationStateNode> node)
        : m_name(name), m_node(node.get()), m_owned(static_cast<std::unique_ptr<IAnimationStateNode>&&>(node)), m_ownsNode(true) {}

    f32  speed = 1.0f;
    bool loop  = true;

    [[nodiscard]] const std::u8string&         name() const noexcept { return m_name; }
    [[nodiscard]] IAnimationStateNode*  node() const noexcept { return m_node; }
    [[nodiscard]] bool                  ownsNode() const noexcept { return m_ownsNode; }
    [[nodiscard]] f32                   duration() const noexcept { return m_node != nullptr ? m_node->duration() : 0.0f; }
private:
    std::u8string                         m_name;
    IAnimationStateNode*           m_node     = nullptr;   // active node (borrowed or == m_owned)
    std::unique_ptr<IAnimationStateNode> m_owned;                // set when owning
    bool                           m_ownsNode = false;
};

enum class LayerBlendMode { Override, Additive };

// A layer: states + transitions + an optional bone mask. Layer 0 is the base; others blend on top.
class AnimationLayer {
public:
    explicit AnimationLayer(std::u8string_view name) : m_name(name) {}

    i32            defaultStateIndex = 0;
    LayerBlendMode blendMode         = LayerBlendMode::Override;
    f32            weight            = 1.0f;

    [[nodiscard]] const std::u8string& name() const noexcept { return m_name; }
    [[nodiscard]] std::vector<std::unique_ptr<AnimationGraphState>>&      states() noexcept { return m_states; }
    [[nodiscard]] std::vector<std::unique_ptr<AnimationGraphTransition>>& transitions() noexcept { return m_transitions; }
    [[nodiscard]] BoneMask* mask() const noexcept { return m_mask.get(); }
    void setMask(std::unique_ptr<BoneMask> mask) { m_mask = static_cast<std::unique_ptr<BoneMask>&&>(mask); }

    i32 addState(std::unique_ptr<AnimationGraphState> state) { const i32 idx = static_cast<i32>(m_states.size()); m_states.push_back(static_cast<std::unique_ptr<AnimationGraphState>&&>(state)); return idx; }
    void addTransition(std::unique_ptr<AnimationGraphTransition> t) { m_transitions.push_back(static_cast<std::unique_ptr<AnimationGraphTransition>&&>(t)); }
    [[nodiscard]] AnimationGraphState* getState(i32 index) const {
        return (index >= 0 && static_cast<usize>(index) < m_states.size()) ? m_states[static_cast<usize>(index)].get() : nullptr;
    }
    [[nodiscard]] i32 findStateIndex(std::u8string_view name) const {
        for (usize i = 0; i < m_states.size(); ++i) { if (m_states[i]->name() == name) { return static_cast<i32>(i); } }
        return -1;
    }
private:
    std::u8string                                       m_name;
    std::vector<std::unique_ptr<AnimationGraphState>>        m_states;
    std::vector<std::unique_ptr<AnimationGraphTransition>>   m_transitions;
    std::unique_ptr<BoneMask>                          m_mask;
};

// Shared graph definition: parameters + layers. Multiple players can reference one graph.
// A resource product (Object) so a cooked AnimationGraphSource can build into it.
class AnimationGraph {
public:
    [[nodiscard]] std::vector<AnimationGraphParameter>&    parameters() noexcept { return m_parameters; }
    [[nodiscard]] const std::vector<AnimationGraphParameter>& parameters() const noexcept { return m_parameters; }
    [[nodiscard]] std::vector<std::unique_ptr<AnimationLayer>>&  layers() noexcept { return m_layers; }

    i32 addParameter(std::u8string_view name, AnimationParameterType type) {
        const i32 idx = static_cast<i32>(m_parameters.size());
        m_parameters.push_back(AnimationGraphParameter{ name, type });
        return idx;
    }
    [[nodiscard]] i32 findParameter(std::u8string_view name) const {
        for (usize i = 0; i < m_parameters.size(); ++i) { if (m_parameters[i].name() == name) { return static_cast<i32>(i); } }
        return -1;
    }
    [[nodiscard]] AnimationGraphParameter* getParameter(i32 index) {
        return (index >= 0 && static_cast<usize>(index) < m_parameters.size()) ? &m_parameters[static_cast<usize>(index)] : nullptr;
    }
    i32 addLayer(std::unique_ptr<AnimationLayer> layer) { const i32 idx = static_cast<i32>(m_layers.size()); m_layers.push_back(static_cast<std::unique_ptr<AnimationLayer>&&>(layer)); return idx; }
private:
    std::vector<AnimationGraphParameter>  m_parameters;
    std::vector<std::unique_ptr<AnimationLayer>> m_layers;
};

// ---- graph player --------------------------------------------------------------------------

// Per-layer runtime state (current/previous state + cross-fade + scratch poses).
struct AnimationGraphLayerRuntime {
    i32  currentStateIndex  = -1;
    f32  currentTime        = 0.0f;
    i32  previousStateIndex = -1;
    f32  previousTime       = 0.0f;
    f32  transitionElapsed  = 0.0f;
    f32  transitionDuration = 0.0f;
    bool isTransitioning    = false;
    std::vector<BoneTransform> poses;
    std::vector<BoneTransform> prevPoses;

    void init(usize boneCount) { poses.resize(boneCount); prevPoses.resize(boneCount); }
    void reset(i32 defaultStateIndex) {
        currentStateIndex = defaultStateIndex; currentTime = 0.0f;
        previousStateIndex = -1; previousTime = 0.0f;
        transitionElapsed = 0.0f; transitionDuration = 0.0f; isTransitioning = false;
    }
};

// Evaluates an AnimationGraph for one skeleton instance (state machine + layer blending).
class AnimationGraphPlayer {
public:
    AnimationGraphPlayer(AnimationGraph& graph, Skeleton& skeleton) : m_graph(&graph), m_skeleton(&skeleton) {
        const usize boneCount = static_cast<usize>(skeleton.boneCount());
        m_finalPoses.resize(boneCount);
        m_skinningMatrices.resize(boneCount);
        m_prevSkinningMatrices.resize(boneCount);
        for (usize i = 0; i < boneCount; ++i) { m_skinningMatrices[i] = math::Matrix4::identity(); m_prevSkinningMatrices[i] = math::Matrix4::identity(); }

        // Runtime parameter copy (each player has independent values).
        for (const AnimationGraphParameter& p : graph.parameters()) { m_parameters.push_back(p); }

        for (auto& layer : graph.layers()) {
            AnimationGraphLayerRuntime rt; rt.init(boneCount); rt.reset(layer->defaultStateIndex);
            m_layerRuntimes.push_back(static_cast<AnimationGraphLayerRuntime&&>(rt));
        }

        // Auto-link blend trees from their stored parameter indices (NodeType tag - no RTTI).
        for (auto& layer : graph.layers()) {
            for (auto& state : layer->states()) {
                IAnimationStateNode* node = state->node();
                if (node == nullptr) { continue; }
                if (node->type() == NodeType::BlendTree1D) {
                    auto* t = static_cast<BlendTree1D*>(node);
                    if (t->parameterIndex >= 0) { m_links1D.push_back(Link1D{ t, t->parameterIndex }); }
                } else if (node->type() == NodeType::BlendTree2D) {
                    auto* t = static_cast<BlendTree2D*>(node);
                    if (t->parameterIndexX >= 0 || t->parameterIndexY >= 0) { m_links2D.push_back(Link2D{ t, t->parameterIndexX, t->parameterIndexY }); }
                }
            }
        }
        resetToBind();
    }

    ~AnimationGraphPlayer() = default;
    AnimationGraphPlayer(const AnimationGraphPlayer&) = delete;
    AnimationGraphPlayer& operator=(const AnimationGraphPlayer&) = delete;

    [[nodiscard]] Skeleton&      getSkeleton() noexcept { return *m_skeleton; }
    [[nodiscard]] AnimationGraph& getGraph() noexcept { return *m_graph; }

    // --- parameter access ---
    void setFloat(i32 i, f32 v)  { if (valid(i)) { m_parameters[static_cast<usize>(i)].floatValue = v; } }
    void setFloat(std::u8string_view n, f32 v) { setFloat(m_graph->findParameter(n), v); }
    [[nodiscard]] f32 getFloat(i32 i) const { return valid(i) ? m_parameters[static_cast<usize>(i)].floatValue : 0.0f; }
    void setInt(i32 i, i32 v)    { if (valid(i)) { m_parameters[static_cast<usize>(i)].intValue = v; } }
    void setInt(std::u8string_view n, i32 v) { setInt(m_graph->findParameter(n), v); }
    [[nodiscard]] i32 getInt(i32 i) const { return valid(i) ? m_parameters[static_cast<usize>(i)].intValue : 0; }
    void setBool(i32 i, bool v)  { if (valid(i)) { m_parameters[static_cast<usize>(i)].boolValue = v; } }
    void setBool(std::u8string_view n, bool v) { setBool(m_graph->findParameter(n), v); }
    [[nodiscard]] bool getBool(i32 i) const { return valid(i) ? m_parameters[static_cast<usize>(i)].boolValue : false; }
    void setTrigger(i32 i)       { if (valid(i)) { m_parameters[static_cast<usize>(i)].boolValue = true; } }
    void setTrigger(std::u8string_view n){ setTrigger(m_graph->findParameter(n)); }

    void setEventHandler(AnimationEventHandler handler) { m_eventHandler = static_cast<AnimationEventHandler&&>(handler); }

    void update(f32 deltaTime) {
        for (usize i = 0; i < m_skinningMatrices.size(); ++i) { m_prevSkinningMatrices[i] = m_skinningMatrices[i]; }
        syncBlendTreeParameters();
        for (usize i = 0; i < m_graph->layers().size() && i < m_layerRuntimes.size(); ++i) {
            updateLayer(*m_graph->layers()[i], m_layerRuntimes[i], deltaTime);
        }
        for (AnimationGraphParameter& p : m_parameters) { p.consumeTrigger(); }
        combineLayers();
        m_matricesDirty = true;
    }

    [[nodiscard]] std::span<const math::Matrix4> getSkinningMatrices() {
        if (m_matricesDirty) {
            m_skeleton->computeSkinningMatrices(std::span<const BoneTransform>{ m_finalPoses.data(), m_finalPoses.size() },
                                                std::span<math::Matrix4>{ m_skinningMatrices.data(), m_skinningMatrices.size() });
            m_matricesDirty = false;
        }
        return std::span<const math::Matrix4>{ m_skinningMatrices.data(), m_skinningMatrices.size() };
    }
    [[nodiscard]] std::span<const math::Matrix4> getPrevSkinningMatrices() const { return { m_prevSkinningMatrices.data(), m_prevSkinningMatrices.size() }; }
    [[nodiscard]] std::span<BoneTransform> getLocalPoses() noexcept { return { m_finalPoses.data(), m_finalPoses.size() }; }

    // --- state query ---
    [[nodiscard]] i32 getCurrentStateIndex(i32 layerIndex = 0) const { return (layerIndex >= 0 && static_cast<usize>(layerIndex) < m_layerRuntimes.size()) ? m_layerRuntimes[static_cast<usize>(layerIndex)].currentStateIndex : -1; }
    [[nodiscard]] bool isTransitioning(i32 layerIndex = 0) const { return (layerIndex >= 0 && static_cast<usize>(layerIndex) < m_layerRuntimes.size()) ? m_layerRuntimes[static_cast<usize>(layerIndex)].isTransitioning : false; }
    [[nodiscard]] f32 getCurrentNormalizedTime(i32 layerIndex = 0) const { return (layerIndex >= 0 && static_cast<usize>(layerIndex) < m_layerRuntimes.size()) ? m_layerRuntimes[static_cast<usize>(layerIndex)].currentTime : 0.0f; }

    void resetToBind() {
        for (i32 i = 0; i < m_skeleton->boneCount() && static_cast<usize>(i) < m_finalPoses.size(); ++i) {
            const Bone* bone = m_skeleton->getBone(i);
            m_finalPoses[static_cast<usize>(i)] = (bone != nullptr) ? bone->localBindPose : BoneTransform{};
        }
        m_matricesDirty = true;
    }
    void forceState(i32 stateIndex, i32 layerIndex = 0) {
        if (layerIndex >= 0 && static_cast<usize>(layerIndex) < m_layerRuntimes.size()) {
            AnimationGraphLayerRuntime& rt = m_layerRuntimes[static_cast<usize>(layerIndex)];
            rt.currentStateIndex = stateIndex; rt.currentTime = 0.0f; rt.isTransitioning = false; rt.previousStateIndex = -1;
        }
    }

private:
    [[nodiscard]] bool valid(i32 i) const noexcept { return i >= 0 && static_cast<usize>(i) < m_parameters.size(); }
    [[nodiscard]] std::span<const AnimationGraphParameter> params() const noexcept { return { m_parameters.data(), m_parameters.size() }; }

    void updateLayer(AnimationLayer& layer, AnimationGraphLayerRuntime& rt, f32 deltaTime) {
        if (rt.currentStateIndex < 0 || static_cast<usize>(rt.currentStateIndex) >= layer.states().size()) { return; }
        AnimationGraphState* currentState = layer.getState(rt.currentStateIndex);

        if (!rt.isTransitioning) { evaluateTransitions(layer, rt); }

        if (rt.isTransitioning) {
            const f32 prevNorm = rt.currentTime;
            advanceStateTime(*currentState, rt.currentTime, deltaTime);
            if (AnimationGraphState* prevState = layer.getState(rt.previousStateIndex)) { advanceStateTime(*prevState, rt.previousTime, deltaTime); }
            currentState = layer.getState(rt.currentStateIndex);   // (re-fetch; index unchanged)
            if (m_eventHandler && currentState != nullptr && currentState->node() != nullptr) {
                currentState->node()->fireEvents(prevNorm, rt.currentTime, currentState->loop, m_eventHandler);
            }
            rt.transitionElapsed += deltaTime;
            if (rt.transitionElapsed >= rt.transitionDuration) { rt.isTransitioning = false; rt.previousStateIndex = -1; }
        } else {
            const f32 prevNorm = rt.currentTime;
            advanceStateTime(*currentState, rt.currentTime, deltaTime);
            if (m_eventHandler && currentState->node() != nullptr) {
                currentState->node()->fireEvents(prevNorm, rt.currentTime, currentState->loop, m_eventHandler);
            }
        }
        sampleLayerPoses(layer, rt);
    }

    void advanceStateTime(AnimationGraphState& state, f32& normalizedTime, f32 deltaTime) {
        if (state.duration() <= 0.0f) { return; }
        normalizedTime += (deltaTime * state.speed) / state.duration();
        if (state.loop) {
            while (normalizedTime >= 1.0f) { normalizedTime -= 1.0f; }
            while (normalizedTime < 0.0f) { normalizedTime += 1.0f; }
        } else {
            normalizedTime = std::clamp(normalizedTime, 0.0f, 1.0f);
        }
    }

    void evaluateTransitions(AnimationLayer& layer, AnimationGraphLayerRuntime& rt) {
        AnimationGraphTransition* best = nullptr;
        i32 bestPriority = 2147483647;
        for (auto& tr : layer.transitions()) {
            if (tr->sourceStateIndex != -1 && tr->sourceStateIndex != rt.currentStateIndex) { continue; }
            if (tr->destStateIndex == rt.currentStateIndex) { continue; }
            if (tr->hasExitTime && rt.currentTime < tr->exitTime) { continue; }
            if (!tr->evaluateConditions(params())) { continue; }
            if (tr->priority < bestPriority) { bestPriority = tr->priority; best = tr.get(); }
        }
        if (best != nullptr) {
            rt.previousStateIndex = rt.currentStateIndex;
            rt.previousTime = rt.currentTime;
            rt.currentStateIndex = best->destStateIndex;
            rt.currentTime = 0.0f;
            rt.transitionElapsed = 0.0f;
            rt.transitionDuration = std::max(best->duration, 0.001f);
            rt.isTransitioning = true;
        }
    }

    void sampleLayerPoses(AnimationLayer& layer, AnimationGraphLayerRuntime& rt) {
        if (rt.currentStateIndex < 0) { return; }
        AnimationGraphState* currentState = layer.getState(rt.currentStateIndex);
        if (currentState == nullptr || currentState->node() == nullptr) { return; }

        if (rt.isTransitioning && rt.previousStateIndex >= 0) {
            AnimationGraphState* prevState = layer.getState(rt.previousStateIndex);
            if (prevState != nullptr && prevState->node() != nullptr) {
                prevState->node()->evaluate(*m_skeleton, rt.previousTime, std::span<BoneTransform>{ rt.prevPoses.data(), rt.prevPoses.size() });
                currentState->node()->evaluate(*m_skeleton, rt.currentTime, std::span<BoneTransform>{ rt.poses.data(), rt.poses.size() });
                const f32 blend = std::clamp(rt.transitionElapsed / rt.transitionDuration, 0.0f, 1.0f);
                blendPoses(std::span<const BoneTransform>{ rt.prevPoses.data(), rt.prevPoses.size() },
                           std::span<const BoneTransform>{ rt.poses.data(), rt.poses.size() }, blend,
                           std::span<BoneTransform>{ rt.poses.data(), rt.poses.size() });
                return;
            }
        }
        currentState->node()->evaluate(*m_skeleton, rt.currentTime, std::span<BoneTransform>{ rt.poses.data(), rt.poses.size() });
    }

    void syncBlendTreeParameters() {
        for (const Link1D& l : m_links1D) { if (valid(l.paramIndex)) { l.tree->parameter = m_parameters[static_cast<usize>(l.paramIndex)].floatValue; } }
        for (const Link2D& l : m_links2D) {
            if (valid(l.paramIndexX)) { l.tree->parameterX = m_parameters[static_cast<usize>(l.paramIndexX)].floatValue; }
            if (valid(l.paramIndexY)) { l.tree->parameterY = m_parameters[static_cast<usize>(l.paramIndexY)].floatValue; }
        }
    }

    void combineLayers() {
        if (m_layerRuntimes.empty()) { return; }
        // Base layer writes directly.
        const AnimationGraphLayerRuntime& base = m_layerRuntimes[0];
        for (usize i = 0; i < m_finalPoses.size() && i < base.poses.size(); ++i) { m_finalPoses[i] = base.poses[i]; }

        for (usize layerIdx = 1; layerIdx < m_layerRuntimes.size() && layerIdx < m_graph->layers().size(); ++layerIdx) {
            AnimationLayer& layer = *m_graph->layers()[layerIdx];
            const AnimationGraphLayerRuntime& rt = m_layerRuntimes[layerIdx];
            if (layer.weight <= 0.0f) { continue; }
            const BoneMask* mask = layer.mask();

            if (layer.blendMode == LayerBlendMode::Override) {
                for (usize b = 0; b < m_finalPoses.size() && b < rt.poses.size(); ++b) {
                    const f32 w = layer.weight * (mask != nullptr ? mask->getWeight(static_cast<i32>(b)) : 1.0f);
                    if (w > 0.0f) { m_finalPoses[b] = BoneTransform::lerp(m_finalPoses[b], rt.poses[b], w); }
                }
            } else {   // Additive: delta from bind pose
                for (usize b = 0; b < m_finalPoses.size() && b < rt.poses.size(); ++b) {
                    const f32 w = layer.weight * (mask != nullptr ? mask->getWeight(static_cast<i32>(b)) : 1.0f);
                    if (w <= 0.0f) { continue; }
                    const Bone* bone = m_skeleton->getBone(static_cast<i32>(b));
                    const BoneTransform bind = (bone != nullptr) ? bone->localBindPose : BoneTransform{};
                    const math::Vector3 deltaPos   = rt.poses[b].position - bind.position;
                    const math::Quaternion deltaRot   = rt.poses[b].rotation * inverse(bind.rotation);
                    const math::Vector3 deltaScale = rt.poses[b].scale / bind.scale;   // component-wise
                    m_finalPoses[b].position = m_finalPoses[b].position + deltaPos * w;
                    m_finalPoses[b].rotation = slerp(math::Quaternion::identity, deltaRot, w) * m_finalPoses[b].rotation;
                    m_finalPoses[b].scale    = m_finalPoses[b].scale * lerp(math::Vector3::one, deltaScale, w);
                }
            }
        }
    }

    struct Link1D { BlendTree1D* tree; i32 paramIndex; };
    struct Link2D { BlendTree2D* tree; i32 paramIndexX; i32 paramIndexY; };

    AnimationGraph* m_graph    = nullptr;
    Skeleton*       m_skeleton = nullptr;
    std::vector<AnimationGraphLayerRuntime> m_layerRuntimes;
    std::vector<AnimationGraphParameter>    m_parameters;        // runtime copy
    std::vector<BoneTransform> m_finalPoses;
    std::vector<math::Matrix4>          m_skinningMatrices;
    std::vector<math::Matrix4>          m_prevSkinningMatrices;
    bool                 m_matricesDirty = true;
    AnimationEventHandler m_eventHandler;
    std::vector<Link1D> m_links1D;
    std::vector<Link2D> m_links2D;
};


} // namespace draco::animation
