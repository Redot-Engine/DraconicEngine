/// Material: the shared, immutable material template (`:material` partition).
///
/// Material: the shared, immutable template - a shader name + variant flags, a list
/// of declared properties, a PipelineConfig, and default values. It is *data*: the
/// MaterialSystem reads the property list to infer the GPU bind-group layout, so a
/// custom material/shader needs no renderer changes. Per-use overrides live in a
/// MaterialInstance.

module;
#include <vector>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <cstring>

export module materials:material;

import core;
import rhi;
import shaders;
import :types;
import :pipeline;

using namespace draco;
namespace rhi = draco::rhi;

export namespace draco::materials {

// Shared material template. Properties are declared once; defaults seed every
// instance. Name strings are owned in a stable backing so property views stay valid.
class Material final {
public:
    std::u8string         name;
    std::u8string         shaderName;
    shaders::ShaderFlags shaderFlags = shaders::ShaderFlags::None;
    PipelineConfig pipeline;

    [[nodiscard]] bool isValid() const noexcept { return shaderName.size() > 0; }
    [[nodiscard]] usize propertyCount() const noexcept { return m_properties.size(); }
    [[nodiscard]] u32 uniformDataSize() const noexcept { return m_uniformDataSize; }

    [[nodiscard]] const MaterialPropertyDef& getProperty(usize index) const noexcept { return m_properties[index]; }

    // Index of the named property, or -1.
    [[nodiscard]] isize getPropertyIndex(std::u8string_view n) const noexcept {
        for (usize i = 0; i < m_properties.size(); ++i) {
            if (m_properties[i].name == n) { return static_cast<isize>(i); }
        }
        return -1;
    }

    [[nodiscard]] const MaterialPropertyDef* findProperty(std::u8string_view n) const noexcept {
        const isize i = getPropertyIndex(n);
        return (i >= 0) ? &m_properties[static_cast<usize>(i)] : nullptr;
    }

    [[nodiscard]] std::span<const MaterialPropertyDef> properties() const noexcept {
        return { m_properties.data(), m_properties.size() };
    }

    // Declares a property. The name is cloned into stable backing so the caller's
    // string need not outlive the material; uniform size grows to fit uniforms.
    void addProperty(const MaterialPropertyDef& prop) {
        std::unique_ptr<std::u8string> owned = std::make_unique<std::u8string>(prop.name);
        MaterialPropertyDef d = prop;
        d.name = *owned;
        m_propertyNames.push_back(std::move(owned));
        m_properties.push_back(d);

        if (d.isUniform()) {
            const u32 end = d.offset + d.size;
            if (end > m_uniformDataSize) { m_uniformDataSize = end; }
        }
    }

    // (Re)allocates the default uniform buffer, preserving existing defaults.
    void allocateDefaultUniformData() {
        if (m_uniformDataSize == 0) { return; }
        if (m_defaultUniformData.size() < m_uniformDataSize) {
            std::vector<u8> grown;
            grown.resize(m_uniformDataSize);
            for (usize i = 0; i < m_defaultUniformData.size(); ++i) { grown[i] = m_defaultUniformData[i]; }
            m_defaultUniformData = std::move(grown);
        }
    }

    void setDefaultFloat(std::u8string_view n, f32 v)        { writeUniform(n, &v, sizeof(v)); }
    void setDefaultFloat2(std::u8string_view n, math::Vector2 v)      { writeUniform(n, &v, sizeof(v)); }
    void setDefaultFloat3(std::u8string_view n, math::Vector3 v)      { writeUniform(n, &v, sizeof(v)); }
    void setDefaultFloat4(std::u8string_view n, math::Vector4 v)      { writeUniform(n, &v, sizeof(v)); }
    void setDefaultColor(std::u8string_view n, math::Vector4 c)       { setDefaultFloat4(n, c); }

    void setDefaultTexture(std::u8string_view n, rhi::TextureView* tex) {
        const isize i = getPropertyIndex(n);
        if (i >= 0 && m_properties[static_cast<usize>(i)].isTexture()) { m_defaultTextures.insert_or_assign(static_cast<usize>(i), tex); }
    }
    void setDefaultSampler(std::u8string_view n, rhi::Sampler* s) {
        const isize i = getPropertyIndex(n);
        if (i >= 0 && m_properties[static_cast<usize>(i)].isSampler()) { m_defaultSamplers.insert_or_assign(static_cast<usize>(i), s); }
    }

    [[nodiscard]] std::span<const u8> defaultUniformData() const noexcept {
        return { m_defaultUniformData.data(), m_defaultUniformData.size() };
    }

    // Overlays a raw default-uniform blob (the cooked/serialized defaults) over the
    // uniform buffer. Used by the resource factory to restore authored defaults.
    void setRawDefaultUniformData(std::span<const u8> data) {
        allocateDefaultUniformData();
        const usize n = data.size() < m_defaultUniformData.size() ? data.size() : m_defaultUniformData.size();
        if (n > 0) { std::memcpy(m_defaultUniformData.data(), data.data(), n); }
    }
    [[nodiscard]] rhi::TextureView* getDefaultTexture(usize propIndex) const noexcept {
        rhi::TextureView* const* p = mapFind(m_defaultTextures, propIndex);
        return (p != nullptr) ? *p : nullptr;
    }
    [[nodiscard]] rhi::Sampler* getDefaultSampler(usize propIndex) const noexcept {
        rhi::Sampler* const* p = mapFind(m_defaultSamplers, propIndex);
        return (p != nullptr) ? *p : nullptr;
    }

private:
    void writeUniform(std::u8string_view n, const void* src, usize bytes) {
        const isize i = getPropertyIndex(n);
        if (i < 0) { return; }
        const MaterialPropertyDef& d = m_properties[static_cast<usize>(i)];
        if (!d.isUniform() || d.offset + bytes > m_defaultUniformData.size()) { return; }
        std::memcpy(m_defaultUniformData.data() + d.offset, src, bytes);
    }

    std::vector<MaterialPropertyDef> m_properties;
    std::vector<std::unique_ptr<std::u8string>>   m_propertyNames;   // stable backing for property name views
    std::vector<u8>                  m_defaultUniformData;
    u32                        m_uniformDataSize = 0;
    std::unordered_map<usize, rhi::TextureView*> m_defaultTextures;
    std::unordered_map<usize, rhi::Sampler*>     m_defaultSamplers;
};


} // namespace draco::materials
