/// A bone/node in the model hierarchy with TRS decomposition.

module;
#include <span>
#include <string_view>

#include <cmath>
#include <string>
#include <vector>

export module model:model_bone;

import core;

using namespace draco;

export namespace draco::model {

/// A bone/node in the model skeleton hierarchy.
class ModelBone {
public:
    ModelBone() = default;
    ~ModelBone() = default;

    [[nodiscard]] std::u8string_view name() const { return std::u8string_view(m_name.data(), m_name.size()); }
    void setName(std::u8string_view n) { m_name = std::u8string(n); }

    /// Add a child bone (non-owning pointer).
    void addChild(ModelBone* child) { m_children.push_back(child); }

    /// Remove all children (does not delete -- children are non-owning).
    void clearChildren() { m_children.clear(); }

    /// Non-owning child pointers (owned by Model::m_bones).
    [[nodiscard]] std::span<ModelBone* const> children() const {
        return std::span<ModelBone* const>(m_children.data(), m_children.size());
    }
    [[nodiscard]] std::span<ModelBone*> children() {
        return std::span<ModelBone*>(m_children.data(), m_children.size());
    }

    /// Update localTransform from translation/rotation/scale (TRS).
    /// Order: Scale -> Rotate -> Translate.
    void updateLocalTransform() {
        // Build scale matrix.
        math::Matrix4 s = math::Matrix4::identity();
        s.m[0][0]  = scale.x;
        s.m[1][1]  = scale.y;
        s.m[2][2] = scale.z;

        // Build rotation matrix from quaternion.
        f32 xx = rotation.x * rotation.x;
        f32 yy = rotation.y * rotation.y;
        f32 zz = rotation.z * rotation.z;
        f32 xy = rotation.x * rotation.y;
        f32 xz = rotation.x * rotation.z;
        f32 yz = rotation.y * rotation.z;
        f32 wx = rotation.w * rotation.x;
        f32 wy = rotation.w * rotation.y;
        f32 wz = rotation.w * rotation.z;

        math::Matrix4 r = math::Matrix4::identity();
        r.m[0][0]  = 1.0f - 2.0f * (yy + zz);
        r.m[0][1]  = 2.0f * (xy + wz);
        r.m[0][2]  = 2.0f * (xz - wy);
        r.m[1][0]  = 2.0f * (xy - wz);
        r.m[1][1]  = 1.0f - 2.0f * (xx + zz);
        r.m[1][2]  = 2.0f * (yz + wx);
        r.m[2][0]  = 2.0f * (xz + wy);
        r.m[2][1]  = 2.0f * (yz - wx);
        r.m[2][2] = 1.0f - 2.0f * (xx + yy);

        // Build translation matrix.
        math::Matrix4 t = math::Matrix4::identity();
        t.m[0][3]  = translation.x;
        t.m[1][3]  = translation.y;
        t.m[2][3] = translation.z;

        // TRS order: Scale -> Rotate -> Translate.
        localTransform = t * (r * s);
    }

    // -- Public fields --

    /// Index of this bone in the model's bone array.
    i32 index = 0;

    /// Parent bone index (-1 if root).
    i32 parentIndex = -1;

    /// Local transform relative to parent.
    math::Matrix4 localTransform = math::Matrix4::identity();

    /// Inverse bind matrix for skinning (mesh space -> bone space).
    math::Matrix4 inverseBindMatrix = math::Matrix4::identity();

    /// Translation component of local transform.
    math::Vector3 translation{};

    /// Rotation component of local transform (quaternion).
    math::Quaternion rotation = math::Quaternion::identity;

    /// Scale component of local transform.
    math::Vector3 scale{ 1, 1, 1 };

    /// Mesh index if this node has a mesh (-1 if none).
    i32 meshIndex = -1;

    /// Skin index if this is a skinned mesh node (-1 if none).
    i32 skinIndex = -1;

private:
    std::u8string m_name;
    std::vector<ModelBone*> m_children;
};

} // namespace draco::model
