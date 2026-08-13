/// A complete 3D model with meshes, materials, skeleton, and animations.

module;
#include <span>
#include <string_view>

#include <limits>
#include <string>
#include <vector>

export module model:model;

import core;
// Re-export the sub-partitions so anything importing `:model` (the IO layer and the
// FBX/GLTF loaders) sees the full model vocabulary through this one partition.
export import :vertex_format;
export import :model_vertex;
export import :mesh_part;
export import :model_texture;
export import :model_material;
export import :model_bone;
export import :model_mesh;
export import :model_animation;
export import :model_skin;

using namespace draco;

export namespace draco::model {

/// Look up `k` in an associative container, returning a pointer to its value or nullptr
/// (a value-or-null find used by the FBX/GLTF loaders' id->index maps).
template <class Map, class Key>
[[nodiscard]] auto mapFind(Map& m, const Key& k) -> decltype(&m.find(k)->second) {
    auto it = m.find(k);
    return it != m.end() ? &it->second : nullptr;
}

/// Result of a model load operation.
enum class ModelLoadResult : u32 {
    Ok,
    FileNotFound,
    ParseError,
    UnsupportedFormat,
    OutOfMemory,
    InvalidData,
};

/// The up axis of a coordinate system, as reported by the source file.
enum class CoordinateAxis : u32 {
    PositiveX,
    NegativeX,
    PositiveY,
    NegativeY,
    PositiveZ,
    NegativeZ,
};

/// A complete 3D model with all owned containers.
class Model {
public:
    Model() = default;

    ~Model() {
        for (auto* m : m_meshes) delete m;
        for (auto* m : m_materials) delete m;
        for (auto* b : m_bones) delete b;
        for (auto* s : m_skins) delete s;
        for (auto* a : m_animations) delete a;
        for (auto* t : m_textures) delete t;
    }

    // Non-copyable, movable.
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&& other) noexcept
        : rootBoneIndex(other.rootBoneIndex),
          originalUpAxis(other.originalUpAxis),
          m_name(static_cast<std::u8string&&>(other.m_name)),
          m_meshes(static_cast<std::vector<ModelMesh*>&&>(other.m_meshes)),
          m_materials(static_cast<std::vector<ModelMaterial*>&&>(other.m_materials)),
          m_bones(static_cast<std::vector<ModelBone*>&&>(other.m_bones)),
          m_skins(static_cast<std::vector<ModelSkin*>&&>(other.m_skins)),
          m_animations(static_cast<std::vector<ModelAnimation*>&&>(other.m_animations)),
          m_textures(static_cast<std::vector<ModelTexture*>&&>(other.m_textures)),
          m_samplers(static_cast<std::vector<TextureSampler>&&>(other.m_samplers)),
          m_bounds(other.m_bounds) {
        other.m_meshes.clear();
        other.m_materials.clear();
        other.m_bones.clear();
        other.m_skins.clear();
        other.m_animations.clear();
        other.m_textures.clear();
    }
    Model& operator=(Model&& other) noexcept {
        if (this != &other) {
            for (auto* m : m_meshes) delete m;
            for (auto* m : m_materials) delete m;
            for (auto* b : m_bones) delete b;
            for (auto* s : m_skins) delete s;
            for (auto* a : m_animations) delete a;
            for (auto* t : m_textures) delete t;

            m_name = static_cast<std::u8string&&>(other.m_name);
            m_meshes = static_cast<std::vector<ModelMesh*>&&>(other.m_meshes);
            m_materials = static_cast<std::vector<ModelMaterial*>&&>(other.m_materials);
            m_bones = static_cast<std::vector<ModelBone*>&&>(other.m_bones);
            m_skins = static_cast<std::vector<ModelSkin*>&&>(other.m_skins);
            m_animations = static_cast<std::vector<ModelAnimation*>&&>(other.m_animations);
            m_textures = static_cast<std::vector<ModelTexture*>&&>(other.m_textures);
            m_samplers = static_cast<std::vector<TextureSampler>&&>(other.m_samplers);
            m_bounds = other.m_bounds;
            rootBoneIndex = other.rootBoneIndex;
            originalUpAxis = other.originalUpAxis;

            other.m_meshes.clear();
            other.m_materials.clear();
            other.m_bones.clear();
            other.m_skins.clear();
            other.m_animations.clear();
            other.m_textures.clear();
        }
        return *this;
    }

    // -- Name --

    [[nodiscard]] std::u8string_view name() const { return std::u8string_view(m_name.data(), m_name.size()); }
    void setName(std::u8string_view n) { m_name = std::u8string(n); }

    // -- Meshes --

    [[nodiscard]] std::span<ModelMesh* const> meshes() const {
        return std::span<ModelMesh* const>(m_meshes.data(), m_meshes.size());
    }
    [[nodiscard]] std::span<ModelMesh*> meshes() {
        return std::span<ModelMesh*>(m_meshes.data(), m_meshes.size());
    }

    /// Add a mesh (takes ownership). Returns index.
    i32 addMesh(ModelMesh* mesh) {
        i32 idx = static_cast<i32>(m_meshes.size());
        m_meshes.push_back(mesh);
        return idx;
    }

    // -- Materials --

    [[nodiscard]] std::span<ModelMaterial* const> materials() const {
        return std::span<ModelMaterial* const>(m_materials.data(), m_materials.size());
    }
    [[nodiscard]] std::span<ModelMaterial*> materials() {
        return std::span<ModelMaterial*>(m_materials.data(), m_materials.size());
    }

    /// Add a material (takes ownership). Returns index.
    i32 addMaterial(ModelMaterial* mat) {
        i32 idx = static_cast<i32>(m_materials.size());
        m_materials.push_back(mat);
        return idx;
    }

    // -- Bones --

    [[nodiscard]] std::span<ModelBone* const> bones() const {
        return std::span<ModelBone* const>(m_bones.data(), m_bones.size());
    }
    [[nodiscard]] std::span<ModelBone*> bones() {
        return std::span<ModelBone*>(m_bones.data(), m_bones.size());
    }

    /// Add a bone (takes ownership). Sets bone.index. Returns index.
    i32 addBone(ModelBone* bone) {
        i32 idx = static_cast<i32>(m_bones.size());
        bone->index = idx;
        m_bones.push_back(bone);
        return idx;
    }

    // -- Skins --

    [[nodiscard]] std::span<ModelSkin* const> skins() const {
        return std::span<ModelSkin* const>(m_skins.data(), m_skins.size());
    }
    [[nodiscard]] std::span<ModelSkin*> skins() {
        return std::span<ModelSkin*>(m_skins.data(), m_skins.size());
    }

    /// Add a skin (takes ownership). Returns index.
    i32 addSkin(ModelSkin* skin) {
        i32 idx = static_cast<i32>(m_skins.size());
        m_skins.push_back(skin);
        return idx;
    }

    // -- Animations --

    [[nodiscard]] std::span<ModelAnimation* const> animations() const {
        return std::span<ModelAnimation* const>(m_animations.data(), m_animations.size());
    }
    [[nodiscard]] std::span<ModelAnimation*> animations() {
        return std::span<ModelAnimation*>(m_animations.data(), m_animations.size());
    }

    /// Add an animation (takes ownership). Returns index.
    i32 addAnimation(ModelAnimation* anim) {
        i32 idx = static_cast<i32>(m_animations.size());
        m_animations.push_back(anim);
        return idx;
    }

    // -- Textures --

    [[nodiscard]] std::span<ModelTexture* const> textures() const {
        return std::span<ModelTexture* const>(m_textures.data(), m_textures.size());
    }
    [[nodiscard]] std::span<ModelTexture*> textures() {
        return std::span<ModelTexture*>(m_textures.data(), m_textures.size());
    }

    /// Add a texture (takes ownership). Returns index.
    i32 addTexture(ModelTexture* tex) {
        i32 idx = static_cast<i32>(m_textures.size());
        m_textures.push_back(tex);
        return idx;
    }

    // -- Samplers --

    [[nodiscard]] std::span<const TextureSampler> samplers() const {
        return std::span<const TextureSampler>(m_samplers.data(), m_samplers.size());
    }
    [[nodiscard]] std::span<TextureSampler> samplers() {
        return std::span<TextureSampler>(m_samplers.data(), m_samplers.size());
    }

    /// Add a sampler. Returns index.
    i32 addSampler(TextureSampler sampler) {
        i32 idx = static_cast<i32>(m_samplers.size());
        m_samplers.push_back(sampler);
        return idx;
    }

    // -- Bounds --

    [[nodiscard]] math::AABB bounds() const { return m_bounds; }

    /// Calculate bounds from all meshes.
    void calculateBounds() {
        if (m_meshes.empty()) {
            m_bounds = math::AABB{};
            return;
        }

        math::Vector3 bmin(std::numeric_limits<f32>::max());
        math::Vector3 bmax(std::numeric_limits<f32>::lowest());

        for (auto* mesh : m_meshes) {
            math::AABB mb = mesh->bounds();
            bmin = min(bmin, mb.min);
            bmax = max(bmax, mb.max);
        }

        m_bounds = math::AABB{bmin, bmax};
    }

    // -- Hierarchy --

    /// Build bone hierarchy from parent indices.
    void buildBoneHierarchy() {
        // Clear existing children.
        for (auto* bone : m_bones)
            bone->clearChildren();

        // Build hierarchy.
        for (usize i = 0; i < m_bones.size(); ++i) {
            auto* bone = m_bones[i];
            if (bone->parentIndex >= 0 &&
                static_cast<usize>(bone->parentIndex) < m_bones.size()) {
                m_bones[bone->parentIndex]->addChild(bone);
            } else if (bone->parentIndex < 0) {
                rootBoneIndex = bone->index;
            }
        }
    }

    // -- Lookup by name --

    /// Get mesh by name (nullptr if not found).
    [[nodiscard]] ModelMesh* getMesh(std::u8string_view n) const {
        for (auto* m : m_meshes)
            if (m->name() == n) return m;
        return nullptr;
    }

    /// Get material by name (nullptr if not found).
    [[nodiscard]] ModelMaterial* getMaterial(std::u8string_view n) const {
        for (auto* m : m_materials)
            if (m->name() == n) return m;
        return nullptr;
    }

    /// Get bone by name (nullptr if not found).
    [[nodiscard]] ModelBone* getBone(std::u8string_view n) const {
        for (auto* b : m_bones)
            if (b->name() == n) return b;
        return nullptr;
    }

    /// Get animation by name (nullptr if not found).
    [[nodiscard]] ModelAnimation* getAnimation(std::u8string_view n) const {
        for (auto* a : m_animations)
            if (a->name() == n) return a;
        return nullptr;
    }

    // -- Public fields --

    /// Index of the root bone (-1 if no hierarchy).
    i32 rootBoneIndex = -1;

    /// The original up axis of the source file's coordinate system.
    CoordinateAxis originalUpAxis = CoordinateAxis::PositiveY;

private:
    std::u8string m_name;
    std::vector<ModelMesh*> m_meshes;
    std::vector<ModelMaterial*> m_materials;
    std::vector<ModelBone*> m_bones;
    std::vector<ModelSkin*> m_skins;
    std::vector<ModelAnimation*> m_animations;
    std::vector<ModelTexture*> m_textures;
    std::vector<TextureSampler> m_samplers;
    math::AABB m_bounds;
};

} // namespace draco::model
