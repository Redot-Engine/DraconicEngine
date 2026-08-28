/// Skin data for skeletal animation.

module;
#include <span>
#include <string_view>

#include <string>
#include <vector>

export module model:model_skin;

import core;

using namespace draco;

export namespace draco::model {

/// Skin data binding joints to inverse bind matrices.
class ModelSkin {
public:
    ModelSkin() = default;
    ~ModelSkin() = default;

    [[nodiscard]] std::u8string_view name() const { return std::u8string_view(m_name.data(), m_name.size()); }
    void setName(std::u8string_view n) { m_name = std::u8string(n); }

    /// Add a joint to the skin.
    void addJoint(i32 boneIndex, math::Matrix4 inverseBindMatrix) {
        m_joints.push_back(boneIndex);
        m_inverseBindMatrices.push_back(inverseBindMatrix);
    }

    [[nodiscard]] std::span<const i32> joints() const {
        return std::span<const i32>(m_joints.data(), m_joints.size());
    }
    [[nodiscard]] std::span<i32> joints() {
        return std::span<i32>(m_joints.data(), m_joints.size());
    }

    [[nodiscard]] std::span<const math::Matrix4> inverseBindMatrices() const {
        return std::span<const math::Matrix4>(m_inverseBindMatrices.data(), m_inverseBindMatrices.size());
    }
    [[nodiscard]] std::span<math::Matrix4> inverseBindMatrices() {
        return std::span<math::Matrix4>(m_inverseBindMatrices.data(), m_inverseBindMatrices.size());
    }

    // -- Public fields --

    /// Index of the skeleton root bone (-1 if not specified).
    i32 skeletonRootIndex = -1;

private:
    std::u8string m_name;
    std::vector<i32> m_joints;
    std::vector<math::Matrix4> m_inverseBindMatrices;
};

} // namespace draco::model
