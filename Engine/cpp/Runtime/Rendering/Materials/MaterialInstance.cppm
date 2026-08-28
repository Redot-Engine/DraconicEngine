/// MaterialInstance: per-use overrides with dirty tracking (`:instance` partition).
///
/// MaterialInstance: a per-use copy of a Material's properties with overridable
/// values and dirty tracking. Setters write into an override uniform buffer / texture
/// maps and flip dirty flags; on a false->true transition the instance NOTIFIES its
/// sink (the MaterialSystem) so re-prep is O(dirty), not O(all instances). The sink
/// is an interface declared here so :system can depend on :instance (not vice-versa),
/// breaking the partition cycle.

module;
#include <vector>
#include <span>
#include <string_view>
#include <unordered_map>
#include <cstring>

export module materials:instance;

import core;
import rhi;
import :types;
import :material;

using namespace draco;
namespace rhi = draco::rhi;

export namespace draco::materials {

class MaterialInstance;

// What a MaterialInstance calls back into (implemented by MaterialSystem). Keeps the
// instance->system edge an interface so the partitions don't import each other.
class IMaterialInstanceSink {
public:
    virtual ~IMaterialInstanceSink() = default;
    virtual void markInstanceDirty(MaterialInstance* instance) = 0;
    virtual void releaseInstance(MaterialInstance* instance) = 0;
};

// Up-to-128-property override bitset.
struct PropertyOverrideMask {
    u64 lo = 0, hi = 0;
    void set(usize i)   noexcept { if (i < 64) lo |= (1ull << i); else if (i < 128) hi |= (1ull << (i - 64)); }
    void clear(usize i) noexcept { if (i < 64) lo &= ~(1ull << i); else if (i < 128) hi &= ~(1ull << (i - 64)); }
    [[nodiscard]] bool isSet(usize i) const noexcept {
        if (i < 64) return (lo & (1ull << i)) != 0;
        if (i < 128) return (hi & (1ull << (i - 64))) != 0;
        return false;
    }
    void reset() noexcept { lo = 0; hi = 0; }
    [[nodiscard]] bool hasAny() const noexcept { return lo != 0 || hi != 0; }
};

// Per-use material with overridable properties + dirty tracking.
class MaterialInstance {
public:
    explicit MaterialInstance(Material* material) : m_material(material) {
        if (material != nullptr && material->uniformDataSize() > 0) {
            m_uniformData.resize(material->uniformDataSize());
            const std::span<const u8> defaults = material->defaultUniformData();
            for (usize i = 0; i < defaults.size() && i < m_uniformData.size(); ++i) { m_uniformData[i] = defaults[i]; }
        }
    }

    ~MaterialInstance() { if (m_sink != nullptr) { m_sink->releaseInstance(this); } }

    MaterialInstance(const MaterialInstance&) = delete;
    MaterialInstance& operator=(const MaterialInstance&) = delete;

    [[nodiscard]] Material* getMaterial() const noexcept { return m_material; }

    // --- property setters (write override + mark dirty) ---
    void setFloat(std::u8string_view n, f32 v)  { writeUniform(n, &v, sizeof(v)); }
    void setFloat2(std::u8string_view n, math::Vector2 v){ writeUniform(n, &v, sizeof(v)); }
    void setFloat3(std::u8string_view n, math::Vector3 v){ writeUniform(n, &v, sizeof(v)); }
    void setFloat4(std::u8string_view n, math::Vector4 v){ writeUniform(n, &v, sizeof(v)); }
    void setColor(std::u8string_view n, math::Vector4 c) { setFloat4(n, c); }

    void setTexture(std::u8string_view n, rhi::TextureView* tex) {
        const isize i = m_material->getPropertyIndex(n);
        if (i < 0 || !m_material->getProperty(static_cast<usize>(i)).isTexture()) { return; }
        m_textures.insert_or_assign(static_cast<usize>(i), tex);
        m_overrides.set(static_cast<usize>(i));
        setBindGroupDirty();
    }
    void setSampler(std::u8string_view n, rhi::Sampler* s) {
        const isize i = m_material->getPropertyIndex(n);
        if (i < 0 || !m_material->getProperty(static_cast<usize>(i)).isSampler()) { return; }
        m_samplers.insert_or_assign(static_cast<usize>(i), s);
        m_overrides.set(static_cast<usize>(i));
        setBindGroupDirty();
    }

    // --- effective values (override else material default) ---
    [[nodiscard]] rhi::TextureView* getTexture(usize propIndex) const {
        if (m_overrides.isSet(propIndex)) {
            rhi::TextureView* const* p = mapFind(m_textures, propIndex);
            if (p != nullptr) { return *p; }
        }
        return m_material->getDefaultTexture(propIndex);
    }
    [[nodiscard]] rhi::Sampler* getSampler(usize propIndex) const {
        if (m_overrides.isSet(propIndex)) {
            rhi::Sampler* const* p = mapFind(m_samplers, propIndex);
            if (p != nullptr) { return *p; }
        }
        return m_material->getDefaultSampler(propIndex);
    }
    [[nodiscard]] std::span<const u8> uniformData() const noexcept {
        return m_uniformData.size() > 0 ? std::span<const u8>{ m_uniformData.data(), m_uniformData.size() }
                                        : m_material->defaultUniformData();
    }

    void resetProperty(std::u8string_view n) {
        const isize idx = m_material->getPropertyIndex(n);
        if (idx < 0) { return; }
        const usize i = static_cast<usize>(idx);
        const MaterialPropertyDef& d = m_material->getProperty(i);
        if (d.isUniform() && m_uniformData.size() >= d.offset + d.size) {
            const std::span<const u8> defaults = m_material->defaultUniformData();
            if (defaults.size() >= d.offset + d.size) { std::memcpy(m_uniformData.data() + d.offset, defaults.data() + d.offset, d.size); }
            setUniformDirty();
        } else if (d.isTexture()) { m_textures.erase(i); setBindGroupDirty(); }
        else if (d.isSampler())   { m_samplers.erase(i); setBindGroupDirty(); }
        m_overrides.clear(i);
    }

    // --- dirty-state (driven by MaterialSystem) ---
    [[nodiscard]] bool isUniformDirty()   const noexcept { return m_uniformDirty; }
    [[nodiscard]] bool isBindGroupDirty() const noexcept { return m_bindGroupDirty; }
    void clearUniformDirty()   noexcept { m_uniformDirty = false; }
    void clearBindGroupDirty() noexcept { m_bindGroupDirty = false; }
    void markUniformDirty()    { setUniformDirty(); }
    void markBindGroupDirty()  { setBindGroupDirty(); }

    // --- wiring used only by MaterialSystem ---
    void setSink(IMaterialInstanceSink* sink) noexcept { m_sink = sink; }
    [[nodiscard]] bool isInDirtyList() const noexcept { return m_inDirtyList; }
    void setInDirtyList(bool v) noexcept { m_inDirtyList = v; }
    [[nodiscard]] rhi::BindGroupLayout* bindGroupLayout() const noexcept { return m_bindGroupLayout; }
    void setBindGroupLayout(rhi::BindGroupLayout* l) noexcept { m_bindGroupLayout = l; }

private:
    void writeUniform(std::u8string_view n, const void* src, usize bytes) {
        const isize idx = m_material->getPropertyIndex(n);
        if (idx < 0) { return; }
        const usize i = static_cast<usize>(idx);
        const MaterialPropertyDef& d = m_material->getProperty(i);
        if (!d.isUniform() || m_uniformData.size() < d.offset + bytes) { return; }
        std::memcpy(m_uniformData.data() + d.offset, src, bytes);
        m_overrides.set(i);
        setUniformDirty();
    }
    void setUniformDirty() {
        if (m_uniformDirty) { return; }
        m_uniformDirty = true;
        if (!m_inDirtyList && m_sink != nullptr) { m_sink->markInstanceDirty(this); }
    }
    void setBindGroupDirty() {
        if (m_bindGroupDirty) { return; }
        m_bindGroupDirty = true;
        if (!m_inDirtyList && m_sink != nullptr) { m_sink->markInstanceDirty(this); }
    }

    Material* m_material = nullptr;                 // borrowed (shared template)
    IMaterialInstanceSink* m_sink = nullptr;        // borrowed (the MaterialSystem)
    std::vector<u8> m_uniformData;                        // override uniform buffer
    std::unordered_map<usize, rhi::TextureView*> m_textures;   // override textures by property index
    std::unordered_map<usize, rhi::Sampler*>     m_samplers;   // override samplers by property index
    PropertyOverrideMask m_overrides;
    rhi::BindGroupLayout* m_bindGroupLayout = nullptr;   // set by MaterialSystem for PSO creation
    bool m_uniformDirty   = true;
    bool m_bindGroupDirty = true;
    bool m_inDirtyList    = false;
};

} // namespace draco::materials
