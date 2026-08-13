/// Skeleton and bones (`:skeleton` partition).
///
/// The skeletal hierarchy A `Bone` is a
/// node with a local bind pose + inverse bind matrix; the `Skeleton` owns the bones and computes
/// world-space + skinning matrices from a set of local poses, evaluated parents-before-children.
///
/// `BoneTransform` reuses `math::Transform` (position/rotation/scale, S*R*T) - byte-for-byte the
/// compact BoneTransform, with Lerp/ToMatrix already in core math.

module;
#include <vector>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

export module animation:skeleton;

import core;

using namespace draco;

export namespace draco::animation {

// Look up `k` in an associative container, returning a pointer to its value or nullptr.
template <class Map, class Key>
[[nodiscard]] auto mapFind(Map& m, const Key& k) -> decltype(&m.find(k)->second) {
    auto it = m.find(k);
    return it != m.end() ? &it->second : nullptr;
}

// A compact per-bone transform for animation (pos/rot/scale). Reuses the core math type.
using BoneTransform = math::Transform;

// One bone in a skeleton hierarchy. Value type owned by the Skeleton's bone array (no per-bone heap
// allocation, unlike the Beef original's Bone[] of references).
struct Bone {
    std::u8string        name;
    i32           index           = 0;
    i32           parentIndex     = -1;             // -1 = root
    BoneTransform localBindPose   = {};             // local transform relative to parent (bind pose)
    math::Matrix4          inverseBindPose = math::Matrix4::identity();// model space -> bone space
    math::Matrix4          rootCorrection  = math::Matrix4::identity();// missing-ancestor transform for roots (e.g. FBX axis conv)
    std::vector<i32>    children;
};

// A resource product (Object), so a cooked SkeletonSource can build into it via the resource system.
class Skeleton {
public:
    Skeleton() = default;
    // Creates `boneCount` default bones with sequential indices (the loader fills the rest).
    explicit Skeleton(i32 boneCount) {
        m_bones.resize(boneCount < 0 ? 0 : static_cast<usize>(boneCount));
        for (usize i = 0; i < m_bones.size(); ++i) { m_bones[i].index = static_cast<i32>(i); }
    }

    Skeleton(const Skeleton&) = delete;
    Skeleton& operator=(const Skeleton&) = delete;

    [[nodiscard]] i32   boneCount() const noexcept { return static_cast<i32>(m_bones.size()); }
    [[nodiscard]] std::vector<Bone>&       bones() noexcept { return m_bones; }
    [[nodiscard]] const std::vector<Bone>& bones() const noexcept { return m_bones; }
    [[nodiscard]] std::span<const i32>    rootBones() const noexcept { return { m_rootBones.data(), m_rootBones.size() }; }
    [[nodiscard]] std::u8string&            name() noexcept { return m_name; }
    [[nodiscard]] const std::u8string&      name() const noexcept { return m_name; }

    // Re-populate this same instance (resource hot-reload keeps outside references valid).
    void clearForReload(i32 boneCount) {
        m_bones.clear(); m_rootBones.clear(); m_hierarchicalOrder.clear(); m_nameMap.clear(); m_name.clear();
        m_bones.resize(boneCount < 0 ? 0 : static_cast<usize>(boneCount));
        for (usize i = 0; i < m_bones.size(); ++i) { m_bones[i].index = static_cast<i32>(i); }
    }

    // Bone index by name, or -1 if not found (requires buildNameMap()).
    [[nodiscard]] i32 findBone(std::u8string_view name) const {
        if (const i32* idx = mapFind(m_nameMap, std::u8string{ name })) { return *idx; }
        return -1;
    }

    [[nodiscard]] Bone*       getBone(i32 index) { return inBounds(index) ? &m_bones[static_cast<usize>(index)] : nullptr; }
    [[nodiscard]] const Bone* getBone(i32 index) const { return inBounds(index) ? &m_bones[static_cast<usize>(index)] : nullptr; }

    // Build the name->index lookup. Call after all bones + names are set.
    void buildNameMap() {
        m_nameMap.clear();
        for (const Bone& b : m_bones) { if (!b.name.empty()) { m_nameMap.insert_or_assign(b.name, b.index); } }
    }

    // Cache root-bone indices (parentIndex < 0). Call after parents are set.
    void findRootBones() {
        m_rootBones.clear();
        for (const Bone& b : m_bones) { if (b.parentIndex < 0) { m_rootBones.push_back(b.index); } }
    }

    // Build each bone's child-index list + the parents-before-children evaluation order. Call after
    // parents are set (and after FindRootBones for a correct hierarchical order).
    void buildChildIndices() {
        const usize n = m_bones.size();
        std::vector<i32> childCounts; childCounts.resize(n);
        for (usize i = 0; i < n; ++i) { childCounts[i] = 0; }
        for (const Bone& b : m_bones) { if (b.parentIndex >= 0 && inBounds(b.parentIndex)) { ++childCounts[static_cast<usize>(b.parentIndex)]; } }

        for (usize i = 0; i < n; ++i) {
            m_bones[i].children.clear();
            m_bones[i].children.reserve(static_cast<usize>(childCounts[i]));
        }
        for (const Bone& b : m_bones) {
            if (b.parentIndex >= 0 && inBounds(b.parentIndex)) { m_bones[static_cast<usize>(b.parentIndex)].children.push_back(b.index); }
        }
        buildHierarchicalOrder();
    }

    // World bind pose -> inverse bind matrix for each bone. Call after local bind poses + parents set.
    void computeInverseBindPoses() {
        const usize n = m_bones.size();
        m_worldScratch.resize(n);
        computeWorldPoses(std::span<const BoneTransform>{}, std::span<math::Matrix4>{ m_worldScratch.data(), n });   // bind pose
        for (usize i = 0; i < n; ++i) { m_bones[i].inverseBindPose = inverse(m_worldScratch[i]); }
    }

    // World-space matrices from local transforms (empty `localPoses` => bind pose), parents first.
    void computeWorldPoses(std::span<const BoneTransform> localPoses, std::span<math::Matrix4> outWorldPoses) {
        if (!m_hierarchicalOrder.empty()) {
            for (i32 boneIndex : m_hierarchicalOrder) { computeBoneWorldPose(boneIndex, localPoses, outWorldPoses); }
        } else {
            for (i32 i = 0; i < boneCount(); ++i) { computeBoneWorldPose(i, localPoses, outWorldPoses); }
        }
    }

    // Final skinning matrices = inverseBindPose * worldPose (row-vector: vertex * skin = v * IBM * world).
    void computeSkinningMatrices(std::span<const BoneTransform> localPoses, std::span<math::Matrix4> outSkinningMatrices) {
        const usize n = m_bones.size();
        m_worldScratch.resize(n);
        computeWorldPoses(localPoses, std::span<math::Matrix4>{ m_worldScratch.data(), n });
        for (usize i = 0; i < n; ++i) {
            if (i < outSkinningMatrices.size()) {
                outSkinningMatrices[i] = m_bones[i].inverseBindPose * m_worldScratch[i];
            }
        }
    }

private:
    [[nodiscard]] bool inBounds(i32 i) const noexcept { return i >= 0 && static_cast<usize>(i) < m_bones.size(); }

    void computeBoneWorldPose(i32 boneIndex, std::span<const BoneTransform> localPoses, std::span<math::Matrix4> outWorldPoses) {
        const usize bi = static_cast<usize>(boneIndex);
        if (!inBounds(boneIndex) || bi >= outWorldPoses.size()) { return; }
        const Bone& bone = m_bones[bi];

        const BoneTransform local = (bi < localPoses.size()) ? localPoses[bi] : bone.localBindPose;
        const math::Matrix4 localMatrix = local.toMatrix();

        if (bone.parentIndex >= 0 && inBounds(bone.parentIndex) && static_cast<usize>(bone.parentIndex) < outWorldPoses.size()) {
            outWorldPoses[bi] = localMatrix * outWorldPoses[static_cast<usize>(bone.parentIndex)];   // child = local * parent
        } else {
            outWorldPoses[bi] = localMatrix * bone.rootCorrection;
        }
    }

    // BFS over the hierarchy so parents precede children; orphans appended at the end.
    void buildHierarchicalOrder() {
        const usize n = m_bones.size();
        m_hierarchicalOrder.clear();
        m_hierarchicalOrder.reserve(n);

        std::vector<i32> queue;
        for (i32 root : m_rootBones) { queue.push_back(root); }
        usize head = 0;
        while (head < queue.size()) {
            const i32 boneIndex = queue[head++];
            m_hierarchicalOrder.push_back(boneIndex);
            if (inBounds(boneIndex)) {
                for (i32 child : m_bones[static_cast<usize>(boneIndex)].children) { queue.push_back(child); }
            }
        }

        // Append any orphans not reached from a root.
        if (m_hierarchicalOrder.size() < n) {
            for (i32 i = 0; i < static_cast<i32>(n); ++i) {
                bool found = false;
                for (i32 ordered : m_hierarchicalOrder) { if (ordered == i) { found = true; break; } }
                if (!found) { m_hierarchicalOrder.push_back(i); }
            }
        }
    }

    std::u8string           m_name;
    std::vector<Bone>      m_bones;
    std::vector<i32>       m_rootBones;
    std::vector<i32>       m_hierarchicalOrder;
    std::unordered_map<std::u8string, i32> m_nameMap;
    std::vector<math::Matrix4>      m_worldScratch;   // reused world-pose scratch (skinning hot path)
};


} // namespace draco::animation
