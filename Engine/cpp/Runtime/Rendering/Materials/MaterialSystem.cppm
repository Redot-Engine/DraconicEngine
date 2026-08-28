/// MaterialSystem: owns instance GPU resources, infers bind-group layouts (`:system` partition).
///
/// MaterialSystem: owns material instances' GPU resources and - the key idea -
/// INFERS the bind-group layout from a material's declared property list (uniforms
/// -> one uniform buffer at binding 0; each texture/sampler -> its own entry). A new
/// material or custom shader therefore needs no renderer changes. Layouts are cached
/// by content hash; instance uniform buffers + bind groups are (re)built lazily, and
/// instances notify the system on dirty so PrepareDirtyInstances is O(dirty).

module;
#include <vector>
#include <span>
#include <string_view>
#include <unordered_map>
#include <cstring>

export module materials:system;

import core;
import rhi;
import :types;
import :material;
import :instance;

using namespace draco;
namespace rhi = draco::rhi;

export namespace draco::materials {

class MaterialSystem final : public IMaterialInstanceSink {
public:
    MaterialSystem() = default;
    ~MaterialSystem() override { shutdown(); }

    MaterialSystem(const MaterialSystem&) = delete;
    MaterialSystem& operator=(const MaterialSystem&) = delete;

    // Binds the system to a device + creates default GPU resources (sampler + white
    // and flat-normal 1x1 fallback textures used when a texture slot is unbound).
    Status initialize(rhi::Device& device) {
        m_device = &device;
        m_queue = device.getQueue(rhi::QueueType::Graphics);
        if (m_queue == nullptr) { return Status{ ErrorCode::Unknown }; }
        return createDefaultResources();
    }

    [[nodiscard]] rhi::Device* device() const noexcept { return m_device; }
    [[nodiscard]] rhi::Sampler* defaultSampler() const noexcept { return m_defaultSampler; }
    [[nodiscard]] rhi::TextureView* whiteTexture() const noexcept { return m_whiteView; }
    [[nodiscard]] rhi::TextureView* normalTexture() const noexcept { return m_normalView; }

    // Ensures the instance's uniform buffer is up to date and returns it (null if the material
    // declares no uniforms). Lets a renderer assemble its own fixed set-2 layout (UBO + textures)
    // while still sourcing the packed uniform data from the material system.
    [[nodiscard]] rhi::Buffer* ensureUniformBuffer(MaterialInstance& instance) {
        Material* material = instance.getMaterial();
        if (material == nullptr || material->uniformDataSize() == 0) { return nullptr; }
        instance.setSink(this);
        if (instance.isUniformDirty()) {
            if (!updateUniformBuffer(instance)) { return nullptr; }
            instance.clearUniformDirty();
        }
        rhi::Buffer** buf = mapFind(m_uniformBuffers, &instance);
        return (buf != nullptr) ? *buf : nullptr;
    }

    // Bind-group layout inferred from the material's property definitions, cached.
    [[nodiscard]] rhi::BindGroupLayout* getOrCreateLayout(Material& material) {
        const u64 hash = computeLayoutHash(material);
        if (rhi::BindGroupLayout** cached = mapFind(m_layoutCache, hash)) { return *cached; }

        std::vector<rhi::BindGroupLayoutEntry> entries;

        bool hasUniforms = false;
        for (const MaterialPropertyDef& p : material.properties()) {
            if (p.isUniform()) { hasUniforms = true; break; }
        }
        if (hasUniforms && material.uniformDataSize() > 0) {
            entries.push_back(rhi::BindGroupLayoutEntry::uniformBuffer(0, rhi::ShaderStage::Fragment));
        }

        u32 texBinding = 0, sampBinding = 0;
        for (const MaterialPropertyDef& p : material.properties()) {
            switch (p.type) {
            case MaterialPropertyType::Texture2D:
                entries.push_back(rhi::BindGroupLayoutEntry::sampledTexture(texBinding++, rhi::ShaderStage::Fragment));
                break;
            case MaterialPropertyType::TextureCube:
                entries.push_back(rhi::BindGroupLayoutEntry::sampledTexture(texBinding++, rhi::ShaderStage::Fragment,
                                                                          rhi::TextureViewDimension::TextureCube));
                break;
            case MaterialPropertyType::Sampler:
                entries.push_back(rhi::BindGroupLayoutEntry::sampler(sampBinding++, rhi::ShaderStage::Fragment));
                break;
            default: break;   // scalars live in the uniform buffer
            }
        }
        if (entries.empty()) { return nullptr; }

        rhi::BindGroupLayoutDesc desc{};
        desc.entries = std::span<const rhi::BindGroupLayoutEntry>{ entries.data(), entries.size() };
        rhi::BindGroupLayout* layout = nullptr;
        if (!m_device->createBindGroupLayout(desc, layout).isOk()) { return nullptr; }
        m_layoutCache.insert_or_assign(hash, layout);
        return layout;
    }

    // (Re)builds an instance's uniform buffer + bind group if dirty. Returns its
    // bind group, ready to bind for rendering.
    [[nodiscard]] rhi::BindGroup* prepareInstance(MaterialInstance& instance, rhi::BindGroupLayout* layout = nullptr) {
        Material* material = instance.getMaterial();
        if (material == nullptr) { return nullptr; }
        instance.setSink(this);

        rhi::BindGroupLayout* bgLayout = layout;
        if (bgLayout == nullptr) { bgLayout = getOrCreateLayout(*material); }
        if (bgLayout == nullptr) { return nullptr; }

        if (instance.isUniformDirty() && material->uniformDataSize() > 0) {
            if (!updateUniformBuffer(instance)) { return nullptr; }
            instance.clearUniformDirty();
        }
        instance.setBindGroupLayout(bgLayout);

        if (instance.isBindGroupDirty()) {
            if (!updateBindGroup(instance, bgLayout)) { return nullptr; }
            instance.clearBindGroupDirty();
        }
        rhi::BindGroup** bg = mapFind(m_bindGroups, &instance);
        return (bg != nullptr) ? *bg : nullptr;
    }

    [[nodiscard]] rhi::BindGroup* getBindGroup(MaterialInstance& instance) {
        rhi::BindGroup** bg = mapFind(m_bindGroups, &instance);
        return (bg != nullptr) ? *bg : nullptr;
    }

    // --- IMaterialInstanceSink: dirty notification + cleanup ---
    void markInstanceDirty(MaterialInstance* instance) override {
        if (instance == nullptr || instance->isInDirtyList()) { return; }
        instance->setInDirtyList(true);
        m_dirty.push_back(instance);
    }

    // Re-preps every instance dirtied since the last drain (O(dirty)).
    void prepareDirtyInstances() {
        for (usize i = 0; i < m_dirty.size(); ++i) {
            MaterialInstance* inst = m_dirty[i];
            if (inst == nullptr) { continue; }
            inst->setInDirtyList(false);
            if (inst->isUniformDirty() || inst->isBindGroupDirty()) { (void)prepareInstance(*inst); }
        }
        m_dirty.clear();
    }

    void releaseInstance(MaterialInstance* instance) override {
        if (instance == nullptr) { return; }
        if (instance->isInDirtyList()) {
            removeFromDirty(instance);
            instance->setInDirtyList(false);
        }
        if (rhi::BindGroup** bg = mapFind(m_bindGroups, instance)) {
            rhi::BindGroup* g = *bg;
            m_device->destroyBindGroup(g);
            m_bindGroups.erase(instance);
        }
        if (rhi::Buffer** buf = mapFind(m_uniformBuffers, instance)) {
            rhi::Buffer* b = *buf;
            m_device->destroyBuffer(b);
            m_uniformBuffers.erase(instance);
        }
    }

    // Sampler cache keyed by address/filter combination.
    [[nodiscard]] rhi::Sampler* getOrCreateSampler(rhi::AddressMode u, rhi::AddressMode v,
                                                   rhi::FilterMode minF = rhi::FilterMode::Linear,
                                                   rhi::FilterMode magF = rhi::FilterMode::Linear,
                                                   rhi::MipmapFilterMode mip = rhi::MipmapFilterMode::Linear) {
        const u64 key = static_cast<u64>(u) | (static_cast<u64>(v) << 4) | (static_cast<u64>(minF) << 8)
                      | (static_cast<u64>(magF) << 12) | (static_cast<u64>(mip) << 16);
        if (rhi::Sampler** cached = mapFind(m_samplerCache, key)) { return *cached; }

        rhi::SamplerDesc desc{};
        desc.addressU = u; desc.addressV = v; desc.addressW = rhi::AddressMode::Repeat;
        desc.minFilter = minF; desc.magFilter = magF; desc.mipmapFilter = mip;
        rhi::Sampler* sampler = nullptr;
        if (!m_device->createSampler(desc, sampler).isOk()) { return m_defaultSampler; }
        m_samplerCache.insert_or_assign(key, sampler);
        return sampler;
    }

private:
    Status createDefaultResources() {
        rhi::SamplerDesc sd{};
        sd.addressU = rhi::AddressMode::ClampToEdge; sd.addressV = rhi::AddressMode::ClampToEdge;
        sd.addressW = rhi::AddressMode::ClampToEdge;
        if (!m_device->createSampler(sd, m_defaultSampler).isOk()) { return Status{ ErrorCode::Unknown }; }

        if (!createTexture1x1(Color32{ 255, 255, 255, 255 }, m_whiteTex, m_whiteView)) { return Status{ ErrorCode::Unknown }; }
        if (!createTexture1x1(Color32{ 128, 128, 255, 255 }, m_normalTex, m_normalView)) { return Status{ ErrorCode::Unknown }; }
        return Status{};
    }

    bool createTexture1x1(Color32 color, rhi::Texture*& outTex, rhi::TextureView*& outView) {
        rhi::TextureDesc td{};
        td.dimension = rhi::TextureDimension::Texture2D;
        td.format = rhi::TextureFormat::RGBA8Unorm;
        td.width = 1; td.height = 1;
        td.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst;
        td.label = u8"1x1";
        if (!m_device->createTexture(td, outTex).isOk()) { return false; }

        const u8 pixel[4] = { color.r, color.g, color.b, color.a };
        rhi::TransferBatch* tb = nullptr;
        if (m_queue->createTransferBatch(tb).isOk() && tb != nullptr) {
            rhi::TextureDataLayout layout{}; layout.bytesPerRow = 4; layout.rowsPerImage = 1;
            tb->writeTexture(outTex, std::span<const u8>{ pixel, 4 }, layout, rhi::Extent3D{ 1, 1, 1 });
            (void)tb->submit();
            m_queue->destroyTransferBatch(tb);
        }

        rhi::TextureViewDesc vd{};
        vd.format = rhi::TextureFormat::RGBA8Unorm;
        vd.dimension = rhi::TextureViewDimension::Texture2D;
        return m_device->createTextureView(outTex, vd, outView).isOk();
    }

    bool updateUniformBuffer(MaterialInstance& instance) {
        Material* material = instance.getMaterial();
        if (material->uniformDataSize() == 0) { return true; }

        rhi::Buffer* buffer = nullptr;
        if (rhi::Buffer** existing = mapFind(m_uniformBuffers, &instance)) {
            buffer = *existing;
        } else {
            rhi::BufferDesc bd{};
            // Round up to 16: an HLSL cbuffer is std140-padded to a 16-byte multiple, so the
            // bound range must cover that even when the declared props sum to less (e.g. a
            // {float4,float,float} PBR block is 24B of data but a 32B cbuffer).
            bd.size = (material->uniformDataSize() + 15u) & ~15u;
            bd.usage = rhi::BufferUsage::Uniform;
            bd.memory = rhi::MemoryLocation::CpuToGpu;
            if (!m_device->createBuffer(bd, buffer).isOk()) { return false; }
            m_uniformBuffers.insert_or_assign(&instance, buffer);
        }

        const std::span<const u8> data = instance.uniformData();
        if (data.size() > 0) {
            if (void* ptr = buffer->map()) {
                std::memcpy(ptr, data.data(), data.size());
                buffer->unmap();
            }
        }
        return true;
    }

    bool updateBindGroup(MaterialInstance& instance, rhi::BindGroupLayout* layout) {
        Material* material = instance.getMaterial();
        std::vector<rhi::BindGroupEntry> entries;

        if (rhi::Buffer** buf = mapFind(m_uniformBuffers, &instance)) {
            entries.push_back(rhi::BindGroupEntry::bufferEntry(*buf, 0, material->uniformDataSize()));
        }

        usize propIndex = 0;
        for (const MaterialPropertyDef& p : material->properties()) {
            if (p.isTexture()) {
                rhi::TextureView* view = instance.getTexture(propIndex);
                if (view == nullptr) {
                    view = (contains(p.name, u8"ormal")) ? m_normalView : m_whiteView;   // *N*ormal/*n*ormal fallback
                }
                if (view != nullptr) { entries.push_back(rhi::BindGroupEntry::textureEntry(view)); }
            } else if (p.isSampler()) {
                rhi::Sampler* s = instance.getSampler(propIndex);
                if (s == nullptr) { s = m_defaultSampler; }
                if (s != nullptr) { entries.push_back(rhi::BindGroupEntry::samplerEntry(s)); }
            }
            ++propIndex;
        }
        if (entries.empty()) { return false; }

        if (rhi::BindGroup** old = mapFind(m_bindGroups, &instance)) {
            rhi::BindGroup* g = *old;
            m_device->destroyBindGroup(g);
            m_bindGroups.erase(&instance);
        }

        rhi::BindGroupDesc desc{};
        desc.layout = layout;
        desc.entries = std::span<const rhi::BindGroupEntry>{ entries.data(), entries.size() };
        rhi::BindGroup* bg = nullptr;
        if (!m_device->createBindGroup(desc, bg).isOk()) { return false; }
        m_bindGroups.insert_or_assign(&instance, bg);
        return true;
    }

    static u64 computeLayoutHash(Material& material) {
        u64 h = 17;
        h = h * 31u + material.uniformDataSize();
        for (const MaterialPropertyDef& p : material.properties()) {
            h = h * 31u + static_cast<u64>(p.type);
            h = h * 31u + p.binding;
        }
        return h;
    }

    // Case-insensitive-ish substring check (matches "normal"/"Normal" via "ormal").
    static bool contains(std::u8string_view haystack, std::u8string_view needle) {
        if (needle.size() == 0 || haystack.size() < needle.size()) { return false; }
        const usize last = haystack.size() - needle.size();
        for (usize i = 0; i <= last; ++i) {
            bool match = true;
            for (usize j = 0; j < needle.size(); ++j) {
                if (haystack[i + j] != needle[j]) { match = false; break; }
            }
            if (match) { return true; }
        }
        return false;
    }

    void removeFromDirty(MaterialInstance* instance) {
        for (usize i = 0; i < m_dirty.size(); ++i) {
            if (m_dirty[i] == instance) { m_dirty[i] = nullptr; return; }
        }
    }

    void shutdown() {
        if (m_device == nullptr) { return; }
        for (auto& e : m_bindGroups)     { m_device->destroyBindGroup(e.second); }
        for (auto& e : m_uniformBuffers) { m_device->destroyBuffer(e.second); }
        for (auto& e : m_layoutCache)    { m_device->destroyBindGroupLayout(e.second); }
        for (auto& e : m_samplerCache)   { m_device->destroySampler(e.second); }
        m_bindGroups.clear(); m_uniformBuffers.clear(); m_layoutCache.clear(); m_samplerCache.clear();

        if (m_whiteView)  { m_device->destroyTextureView(m_whiteView); }
        if (m_normalView) { m_device->destroyTextureView(m_normalView); }
        if (m_whiteTex)   { m_device->destroyTexture(m_whiteTex); }
        if (m_normalTex)  { m_device->destroyTexture(m_normalTex); }
        if (m_defaultSampler) { m_device->destroySampler(m_defaultSampler); }
        m_device = nullptr;
    }

    rhi::Device* m_device = nullptr;
    rhi::Queue*  m_queue = nullptr;
    rhi::Sampler* m_defaultSampler = nullptr;
    rhi::Texture* m_whiteTex = nullptr;  rhi::TextureView* m_whiteView = nullptr;
    rhi::Texture* m_normalTex = nullptr; rhi::TextureView* m_normalView = nullptr;

    std::unordered_map<u64, rhi::BindGroupLayout*> m_layoutCache;
    std::unordered_map<u64, rhi::Sampler*>         m_samplerCache;
    std::unordered_map<MaterialInstance*, rhi::Buffer*>    m_uniformBuffers;
    std::unordered_map<MaterialInstance*, rhi::BindGroup*> m_bindGroups;
    std::vector<MaterialInstance*> m_dirty;
};

} // namespace draco::materials
