/// Render-side PSO cache with shader version polling (the `materials.pso` module).
///
/// The render-side PSO cache: the one piece of the shader/material stack that lives
/// outside the resource system (the "lone exception" from the hot-reload design). It
/// maps a PipelineConfig (× render-target signature) to a compiled RHI RenderPipeline,
/// pulling shader variants from the ShaderSystem and mapping the material's render
/// state to RHI pipeline state.
///
/// Hot reload is handled by VERSION POLLING at point of use: each cached entry records
/// the ShaderSystem version of its shader at build time; GetPipeline compares against
/// the current version and lazily rebuilds when a shader was invalidated (reloaded).
/// This costs one HashMap lookup + integer compare per draw - the same as a dirty-flag
/// check - with no listener bookkeeping. Superseded pipelines are retired to a
/// graveyard and freed by releaseRetired() once the GPU is done with them.

module;
#include <vector>
#include <span>
#include <unordered_map>
#include <optional>
#include <algorithm>

export module materials.pso;

import core;
import rhi;
import shaders;
import shaders.system;
import materials;

using namespace draco;
namespace rhi = draco::rhi;

export namespace draco::materials {

class PipelineStateCache {
public:
    PipelineStateCache(shaders::ShaderSystem& shaderSystem, rhi::Device& device) noexcept
        : m_shaders(&shaderSystem), m_device(&device) {}

    ~PipelineStateCache() { clear(); releaseRetired(); }

    PipelineStateCache(const PipelineStateCache&) = delete;
    PipelineStateCache& operator=(const PipelineStateCache&) = delete;

    // Returns a RenderPipeline for (config, layout, colorOverride), building it on the
    // first request and rebuilding it when the shader has since been reloaded. `layout`
    // is the assembled pipeline layout (frame/object/material bind-group layouts); the
    // caller owns it. Returns null on build failure.
    [[nodiscard]] rhi::RenderPipeline* getPipeline(const PipelineConfig& config, rhi::PipelineLayout* layout,
                                                   rhi::TextureFormat colorOverride = rhi::TextureFormat::Undefined) {
        const u64 key = keyOf(config, layout, colorOverride);
        const u64 version = m_shaders->version(config.shaderName);

        if (Entry* e = mapFind(m_entries, key)) {
            if (e->builtVersion == version && e->pipeline != nullptr) { return e->pipeline; }
            // shader was reloaded since this PSO was built: retire the stale one + rebuild.
            if (e->pipeline != nullptr) { m_retired.push_back(e->pipeline); }
            e->pipeline = build(config, layout, colorOverride);
            e->builtVersion = version;
            return e->pipeline;
        }

        Entry entry{};
        entry.pipeline = build(config, layout, colorOverride);
        entry.builtVersion = version;
        rhi::RenderPipeline* result = entry.pipeline;
        m_entries.insert_or_assign(key, entry);
        return result;
    }

    [[nodiscard]] usize size() const noexcept { return m_entries.size(); }
    [[nodiscard]] usize retiredCount() const noexcept { return m_retired.size(); }

    // Frees pipelines superseded by a reload. Call once the frames that may still
    // reference them have completed (the GraphicsDevice frame ring is the gate).
    void releaseRetired() {
        for (rhi::RenderPipeline* p : m_retired) { if (p != nullptr) { m_device->destroyRenderPipeline(p); } }
        m_retired.clear();
    }

    // Destroys all live pipelines (retires nothing - call at shutdown).
    void clear() {
        for (auto& e : m_entries) { if (e.second.pipeline != nullptr) { m_device->destroyRenderPipeline(e.second.pipeline); } }
        m_entries.clear();
    }

private:
    struct Entry {
        rhi::RenderPipeline* pipeline = nullptr;
        u64                  builtVersion = 0;
    };

    static u64 keyOf(const PipelineConfig& config, rhi::PipelineLayout* layout, rhi::TextureFormat colorOverride) {
        u64 h = config.hashCode();
        h = h * 31u + reinterpret_cast<u64>(layout);
        h = h * 31u + static_cast<u64>(colorOverride);
        return h;
    }

    rhi::RenderPipeline* build(const PipelineConfig& config, rhi::PipelineLayout* layout, rhi::TextureFormat colorOverride) {
        rhi::ShaderModule* vs = m_shaders->getVariant(config.shaderName, shaders::ShaderStage::Vertex, config.shaderFlags);
        if (vs == nullptr) { return nullptr; }

        rhi::RenderPipelineDesc desc{};
        desc.layout = layout;
        desc.label = config.shaderName;

        // --- vertex ---
        // Up to three buffers, in slot order matching the renderer's draw bindings: mesh stream
        // (slot 0); the skinning stream (slot 1, joints loc 6 + weights loc 7) when skinned; the
        // instance-stepped DataOffsets stream (loc 5) when instanced. Skinned draws are always
        // instanced -> [mesh, skin, offsets]; non-skinned instanced -> [mesh, offsets].
        rhi::VertexBufferLayout buffers[3] = { VertexLayoutHelper::bufferLayout(config.vertexLayout), {}, {} };
        u32 bufferCount = (config.vertexLayout != VertexLayoutType::None) ? 1u : 0u;
        if (config.vertexLayout == VertexLayoutType::SkinnedMesh) { buffers[bufferCount++] = VertexLayoutHelper::skinningStreamBufferLayout(); }
        if (config.instanced) { buffers[bufferCount++] = VertexLayoutHelper::instanceOffsetsBufferLayout(); }
        desc.vertex.shader = rhi::ProgrammableStage{ vs, u8"main", rhi::ShaderStage::Vertex };
        if (bufferCount > 0) { desc.vertex.buffers = std::span<const rhi::VertexBufferLayout>{ buffers, bufferCount }; }

        // --- fragment (omitted for depth-only passes) ---
        // Up to colorTargetCount targets (MRT): target 0 is the shaded color (blended per blendMode +
        // format from colorOverride when set, e.g. the per-view HDR/LDR format); targets 1+ are the
        // G-buffer aux outputs (view-normal, motion vector) - no blend, formats from config.colorFormats.
        rhi::ColorTargetState colorTargets[rhi::maxColorAttachments] = {};
        if (!config.depthOnly) {
            rhi::ShaderModule* fs = m_shaders->getVariant(config.shaderName, shaders::ShaderStage::Fragment, config.shaderFlags);
            if (fs == nullptr) { return nullptr; }
            // Honor colorTargetCount exactly - 0 means a fragment that writes no color (e.g. the masked
            // shadow pass: alpha-test discard + depth only). No fallback to 1.
            const u32 count = std::min<u32>(config.colorTargetCount, rhi::maxColorAttachments);
            for (u32 i = 0; i < count; ++i) {
                colorTargets[i].format    = (i == 0 && colorOverride != rhi::TextureFormat::Undefined) ? colorOverride : config.colorFormats[i];
                colorTargets[i].blend     = (i == 0) ? blendFor(config.blendMode) : std::optional<rhi::BlendState>{};
                // Aux G-buffer targets (1+) only write when writeAuxTargets is set - transparent draws
                // bind them (to match the pass) but leave the opaque normal/velocity underneath intact.
                colorTargets[i].writeMask = (i == 0 || config.writeAuxTargets) ? config.colorWriteMask : rhi::ColorWriteMask::None;
            }
            rhi::FragmentState frag{};
            frag.shader = rhi::ProgrammableStage{ fs, u8"main", rhi::ShaderStage::Fragment };
            frag.targets = std::span<const rhi::ColorTargetState>{ colorTargets, count };
            desc.fragment = frag;
        }

        // --- primitive ---
        desc.primitive.topology  = config.topology;
        desc.primitive.frontFace = config.frontFace;
        desc.primitive.cullMode  = cullFor(config.cullMode);
        desc.primitive.fillMode  = config.fillMode;

        // --- depth/stencil ---
        if (config.depthMode != DepthMode::Disabled) {
            rhi::DepthStencilState ds{};
            ds.format = config.depthFormat;
            ds.depthTestEnabled  = config.depthMode == DepthMode::ReadWrite || config.depthMode == DepthMode::ReadOnly;
            ds.depthWriteEnabled = config.depthMode == DepthMode::ReadWrite || config.depthMode == DepthMode::WriteOnly;
            ds.depthCompare = config.depthCompare;
            ds.depthBias = config.depthBias;
            ds.depthBiasSlopeScale = config.depthBiasSlopeScale;
            desc.depthStencil = ds;
        }

        // --- multisample ---
        desc.multisample.count = config.sampleCount;

        rhi::RenderPipeline* pipeline = nullptr;
        if (!m_device->createRenderPipeline(desc, pipeline).isOk()) { return nullptr; }
        return pipeline;
    }

    static std::optional<rhi::BlendState> blendFor(BlendMode mode) {
        switch (mode) {
        case BlendMode::Opaque:
        case BlendMode::Masked:
            return {};   // no blending
        case BlendMode::AlphaBlend:
            return rhi::BlendState::alphaBlend();
        case BlendMode::Additive:
            return rhi::BlendState{ { rhi::BlendFactor::One, rhi::BlendFactor::One, rhi::BlendOperation::Add },
                                    { rhi::BlendFactor::One, rhi::BlendFactor::One, rhi::BlendOperation::Add } };
        case BlendMode::Multiply:
            return rhi::BlendState{ { rhi::BlendFactor::Dst, rhi::BlendFactor::Zero, rhi::BlendOperation::Add },
                                    { rhi::BlendFactor::Dst, rhi::BlendFactor::Zero, rhi::BlendOperation::Add } };
        case BlendMode::PremultipliedAlpha:
            return rhi::BlendState{ { rhi::BlendFactor::One, rhi::BlendFactor::OneMinusSrcAlpha, rhi::BlendOperation::Add },
                                    { rhi::BlendFactor::One, rhi::BlendFactor::OneMinusSrcAlpha, rhi::BlendOperation::Add } };
        }
        return {};
    }

    static rhi::CullMode cullFor(CullModeConfig c) {
        switch (c) {
        case CullModeConfig::None:  return rhi::CullMode::None;
        case CullModeConfig::Back:  return rhi::CullMode::Back;
        case CullModeConfig::Front: return rhi::CullMode::Front;
        }
        return rhi::CullMode::None;
    }

    shaders::ShaderSystem* m_shaders;   // borrowed
    rhi::Device*           m_device;    // borrowed
    std::unordered_map<u64, Entry>          m_entries;
    std::vector<rhi::RenderPipeline*>  m_retired;   // superseded by reload, awaiting GPU-safe free
};

} // namespace draco::materials
