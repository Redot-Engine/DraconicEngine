// Animation graph stack: parameters, conditions, transitions, states, layers, blend trees, graph,
// and a graph-player smoke test, plus a state-machine integration check.
#include <doctest_with_main.h>
#include <vector>
#include <span>
#include <string_view>
#include <memory>

import core;
import animation;

using namespace draco;
using namespace draco::animation;

static IAnimationStateNode* kNullNode = static_cast<IAnimationStateNode*>(nullptr);

// ---- BoneMask ----
TEST_CASE("graph: BoneMask weights, clamping, out-of-range")
{
    BoneMask mask{ 4, 1.0f };
    CHECK(mask.boneCount() == 4);
    CHECK(mask.getWeight(0) == 1.0f);
    mask.setWeight(2, 0.75f);
    CHECK(mask.getWeight(2) == 0.75f);
    mask.setWeight(0, 2.0f);  CHECK(mask.getWeight(0) == 1.0f);   // clamp high
    mask.setWeight(1, -1.0f); CHECK(mask.getWeight(1) == 0.0f);   // clamp low
    CHECK(mask.getWeight(-1) == 0.0f);   // out of range
    CHECK(mask.getWeight(99) == 0.0f);
    BoneMask m2{ 2, 0.0f };
    m2.setAll(5.0f); CHECK(m2.getWeight(0) == 1.0f);              // SetAll clamps
}

TEST_CASE("graph: BoneMask SetBoneChainWeight follows hierarchy")
{
    Skeleton s{ 3 };
    s.bones()[0].index = 0; s.bones()[0].parentIndex = -1;
    s.bones()[1].index = 1; s.bones()[1].parentIndex = 0;
    s.bones()[2].index = 2; s.bones()[2].parentIndex = 1;
    s.findRootBones(); s.buildChildIndices();
    BoneMask mask{ 3, 0.0f };
    mask.setBoneChainWeight(s, 0, 1.0f);   // root + all descendants
    CHECK(mask.getWeight(0) == 1.0f);
    CHECK(mask.getWeight(1) == 1.0f);
    CHECK(mask.getWeight(2) == 1.0f);
}

// ---- AnimationGraphParameter ----
TEST_CASE("graph: parameter set/get + trigger consume")
{
    AnimationGraphParameter sp{ u8"Speed", AnimationParameterType::Float };
    CHECK(sp.floatValue == 0.0f);
    CHECK(sp.type == AnimationParameterType::Float);
    CHECK(sp.name() == std::u8string_view{ u8"Speed" });
    sp.floatValue = 1.5f; CHECK(sp.floatValue == 1.5f);

    AnimationGraphParameter trig{ u8"Fire", AnimationParameterType::Trigger };
    trig.boolValue = true;
    trig.consumeTrigger(); CHECK(trig.boolValue == false);   // trigger resets

    AnimationGraphParameter b{ u8"Flag", AnimationParameterType::Bool };
    b.boolValue = true; b.consumeTrigger(); CHECK(b.boolValue == true);   // bool not affected
}

// ---- AnimationGraphCondition ----
TEST_CASE("graph: condition float/int/bool comparisons")
{
    AnimationGraphParameter sp{ u8"Speed", AnimationParameterType::Float };
    sp.floatValue = 0.5f;
    CHECK(AnimationGraphCondition{ 0, ComparisonOp::Greater, 0.1f }.evaluate(&sp) == true);
    sp.floatValue = 0.05f;
    CHECK(AnimationGraphCondition{ 0, ComparisonOp::Greater, 0.1f }.evaluate(&sp) == false);
    sp.floatValue = 0.1f;
    CHECK(AnimationGraphCondition{ 0, ComparisonOp::LessEqual, 0.1f }.evaluate(&sp) == true);
    sp.floatValue = 1.0f;
    CHECK(AnimationGraphCondition{ 0, ComparisonOp::Equal, 1.0f }.evaluate(&sp) == true);

    AnimationGraphParameter lvl{ u8"Level", AnimationParameterType::Int };
    lvl.intValue = 5;
    CHECK(AnimationGraphCondition{ 0, ComparisonOp::Greater, 3.0f }.evaluate(&lvl) == true);

    AnimationGraphParameter g{ u8"Grounded", AnimationParameterType::Bool };
    g.boolValue = true;
    CHECK(AnimationGraphCondition{ 0, ComparisonOp::Equal, 1.0f }.evaluate(&g) == true);    // "is true"
    g.boolValue = false;
    CHECK(AnimationGraphCondition{ 0, ComparisonOp::Equal, 0.0f }.evaluate(&g) == true);    // "is false"

    CHECK(AnimationGraphCondition{ 0, ComparisonOp::Greater, 0.0f }.evaluate(nullptr) == false);
}

// ---- AnimationGraphTransition ----
TEST_CASE("graph: transition condition evaluation")
{
    std::vector<AnimationGraphParameter> params;
    params.push_back(AnimationGraphParameter{ u8"Speed", AnimationParameterType::Float });
    params.push_back(AnimationGraphParameter{ u8"Grounded", AnimationParameterType::Bool });
    params[0].floatValue = 1.0f;
    params[1].boolValue = true;
    const std::span<const AnimationGraphParameter> pv{ params.data(), params.size() };

    AnimationGraphTransition t;
    CHECK(t.sourceStateIndex == -1);    // defaults
    CHECK(t.duration == 0.25f);
    CHECK(t.evaluateConditions(pv) == true);   // no conditions -> unconditional

    t.addFloatCondition(0, ComparisonOp::Greater, 0.1f);
    t.addBoolCondition(1, true);
    CHECK(t.evaluateConditions(pv) == true);    // both met
    params[1].boolValue = false;
    CHECK(t.evaluateConditions(pv) == false);   // one fails

    AnimationGraphTransition bad;
    bad.addFloatCondition(5, ComparisonOp::Greater, 0.0f);   // index out of range
    CHECK(bad.evaluateConditions(pv) == false);
}

// ---- AnimationGraphState ----
TEST_CASE("graph: state defaults + node ownership")
{
    AnimationGraphState idle{ u8"Idle", kNullNode };
    CHECK(idle.name() == std::u8string_view{ u8"Idle" });
    CHECK(idle.node() == nullptr);
    CHECK(idle.speed == 1.0f);
    CHECK(idle.loop == true);
    CHECK(idle.ownsNode() == false);
    CHECK(idle.duration() == 0.0f);

    // Owned node freed on destruction (UniquePtr) - no leak/crash.
    {
        AnimationGraphState owned{ u8"Owned", std::make_unique<ClipStateNode>(static_cast<AnimationClip*>(nullptr)) };
        CHECK(owned.ownsNode() == true);
    }
    // Borrowed node survives the state.
    ClipStateNode node{ nullptr };
    { AnimationGraphState borrow{ u8"Borrow", &node }; CHECK(borrow.ownsNode() == false); }
    CHECK(node.duration() == 0.0f);
}

// ---- AnimationLayer ----
TEST_CASE("graph: layer states/transitions/mask")
{
    AnimationLayer layer{ u8"Base" };
    CHECK(layer.name() == std::u8string_view{ u8"Base" });
    CHECK(layer.defaultStateIndex == 0);
    CHECK(layer.blendMode == LayerBlendMode::Override);
    CHECK(layer.weight == 1.0f);
    CHECK(layer.mask() == nullptr);

    CHECK(layer.addState(std::make_unique<AnimationGraphState>(std::u8string_view{ u8"Idle" }, kNullNode)) == 0);
    CHECK(layer.addState(std::make_unique<AnimationGraphState>(std::u8string_view{ u8"Walk" }, kNullNode)) == 1);
    CHECK(layer.states().size() == 2);
    CHECK(layer.getState(1)->name() == std::u8string_view{ u8"Walk" });
    CHECK(layer.getState(-1) == nullptr);
    CHECK(layer.getState(99) == nullptr);
    CHECK(layer.findStateIndex(u8"Walk") == 1);

    layer.addTransition(std::make_unique<AnimationGraphTransition>());
    CHECK(layer.transitions().size() == 1);

    layer.setMask(std::make_unique<BoneMask>(4, 0.0f));
    layer.mask()->setWeight(0, 1.0f);
    CHECK(layer.mask()->getWeight(0) == 1.0f);
    CHECK(layer.mask()->getWeight(1) == 0.0f);
}

// ---- BlendTree1D / 2D ----
TEST_CASE("graph: BlendTree1D sorts by threshold, duration, empty-safe")
{
    BlendTree1D tree;
    tree.addEntry(1.0f, nullptr);
    tree.addEntry(0.0f, nullptr);
    tree.addEntry(0.5f, nullptr);
    CHECK(tree.entries().size() == 3);
    CHECK(tree.entries()[0].threshold == 0.0f);
    CHECK(tree.entries()[1].threshold == 0.5f);
    CHECK(tree.entries()[2].threshold == 1.0f);
    CHECK(tree.parameter == 0.0f);
    CHECK(tree.duration() == 0.0f);   // null clips
    tree.addEntry(0.5f, nullptr); CHECK(tree.entries().size() == 4);   // duplicates allowed

    Skeleton empty{ 0 };
    BoneTransform poses[4] = {};
    BlendTree1D blank;
    blank.evaluate(empty, 0.0f, std::span<BoneTransform>{ poses, 4 });   // empty -> no crash
}

TEST_CASE("graph: BlendTree2D entries + positions + duration")
{
    BlendTree2D tree;
    tree.addEntry(0.0f, 0.0f, nullptr);
    tree.addEntry(math::Vector2{ 1, 0 }, nullptr);
    tree.addEntry(-1.0f, 2.5f, nullptr);
    CHECK(tree.entries().size() == 3);
    CHECK(tree.entries()[1].position.x == 1.0f);
    CHECK(tree.entries()[2].position.x == -1.0f);
    CHECK(tree.entries()[2].position.y == 2.5f);
    CHECK(tree.parameterX == 0.0f);
    CHECK(tree.duration() == 0.0f);
}

// ---- AnimationGraph ----
TEST_CASE("graph: parameters + layers + find (case sensitive)")
{
    AnimationGraph graph;
    CHECK(graph.addParameter(u8"Speed", AnimationParameterType::Float) == 0);
    CHECK(graph.addParameter(u8"Grounded", AnimationParameterType::Bool) == 1);
    CHECK(graph.parameters().size() == 2);
    CHECK(graph.findParameter(u8"Speed") == 0);
    CHECK(graph.findParameter(u8"Missing") == -1);
    CHECK(graph.findParameter(u8"speed") == -1);   // case sensitive
    CHECK(graph.getParameter(0)->type == AnimationParameterType::Float);
    CHECK(graph.getParameter(-1) == nullptr);

    CHECK(graph.addLayer(std::make_unique<AnimationLayer>(std::u8string_view{ u8"Base" })) == 0);
    CHECK(graph.addLayer(std::make_unique<AnimationLayer>(std::u8string_view{ u8"Upper" })) == 1);
    CHECK(graph.layers().size() == 2);
}

// ---- AnimationGraphPlayer (state-machine integration) ----
TEST_CASE("graph: player transitions states on a bool parameter")
{
    Skeleton skel{ 1 };
    skel.bones()[0].index = 0; skel.bones()[0].parentIndex = -1;
    skel.findRootBones(); skel.buildChildIndices(); skel.computeInverseBindPoses();

    // Two clips so states have non-zero duration (the player advances normalized time by dt/duration).
    AnimationClip idleClip{ u8"idle", 1.0f, true };
    idleClip.getOrCreatePositionTrack(0)->addKeyframe(0.0f, math::Vector3{ 0, 0, 0 });
    idleClip.getOrCreatePositionTrack(0)->addKeyframe(1.0f, math::Vector3{ 0, 0, 0 });
    AnimationClip walkClip{ u8"walk", 1.0f, true };
    walkClip.getOrCreatePositionTrack(0)->addKeyframe(0.0f, math::Vector3{ 0, 0, 0 });
    walkClip.getOrCreatePositionTrack(0)->addKeyframe(1.0f, math::Vector3{ 0, 0, 0 });

    AnimationGraph graph;
    const i32 pMoving = graph.addParameter(u8"Moving", AnimationParameterType::Bool);
    auto layer = std::make_unique<AnimationLayer>(std::u8string_view{ u8"Base" });
    layer->addState(std::make_unique<AnimationGraphState>(std::u8string_view{ u8"Idle" },
                    std::make_unique<ClipStateNode>(&idleClip)));   // state 0
    layer->addState(std::make_unique<AnimationGraphState>(std::u8string_view{ u8"Walk" },
                    std::make_unique<ClipStateNode>(&walkClip)));   // state 1
    auto toWalk = std::make_unique<AnimationGraphTransition>();
    toWalk->sourceStateIndex = 0; toWalk->destStateIndex = 1; toWalk->duration = 0.001f;
    toWalk->addBoolCondition(pMoving, true);
    layer->addTransition(static_cast<std::unique_ptr<AnimationGraphTransition>&&>(toWalk));
    graph.addLayer(static_cast<std::unique_ptr<AnimationLayer>&&>(layer));

    AnimationGraphPlayer player{ graph, skel };
    CHECK(player.getCurrentStateIndex() == 0);    // starts at default (Idle)

    player.update(0.016f);
    CHECK(player.getCurrentStateIndex() == 0);    // condition false -> stays

    player.setBool(pMoving, true);
    player.update(0.016f);
    CHECK(player.getCurrentStateIndex() == 1);    // transitioned to Walk
    std::span<const math::Matrix4> skin = player.getSkinningMatrices();
    CHECK(skin.size() == 1);
}
