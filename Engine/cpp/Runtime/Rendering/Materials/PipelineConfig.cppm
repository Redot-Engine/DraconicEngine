/// PipelineConfig: full render-state description + PSO-cache key (`:pipeline` partition).
///
/// PipelineConfig: the full render-state description for a material (shader name +
/// variant flags, vertex layout, primitive/blend/depth state, render-target
/// formats). It is the *key* a PSO cache hashes on - orthogonal to ShaderFlags:
/// flags drive the shader permutation (compile-time #defines), PipelineConfig
/// drives the fixed-function state. All value types so it hashes by content.
///
/// VertexLayoutHelper maps the predefined VertexLayoutType presets to concrete RHI
/// vertex attributes / strides for the predefined layouts.

module;
#include <span>
#include <string_view>
#include <functional>

export module materials:pipeline;

import core;
import rhi;
import shaders;
import :types;

using namespace draco;
namespace rhi = draco::rhi;

export namespace draco::materials {

// Full render-state for a material. Content-hashable PSO-cache key.
struct PipelineConfig {
    // --- shader identification ---
    std::u8string_view           shaderName;
    shaders::ShaderFlags shaderFlags = shaders::ShaderFlags::None;

    // --- vertex input ---
    VertexLayoutType vertexLayout        = VertexLayoutType::Mesh;
    u32              customVertexStride  = 0;
    u8               customAttributeCount = 0;
    // When set, the PSO gains a second, instance-stepped vertex buffer: a uint4 DataOffsets
    // stream (location 5) whose .x indexes a per-instance StructuredBuffer. Pair with
    // ShaderFlags::Instanced (the shader's INSTANCED permutation reads it).
    bool             instanced            = false;

    // --- primitive assembly ---
    rhi::PrimitiveTopology topology  = rhi::PrimitiveTopology::TriangleList;
    CullModeConfig         cullMode  = CullModeConfig::Back;
    rhi::FrontFace         frontFace = rhi::FrontFace::CCW;   // CCW after Y-flip in projection
    rhi::FillMode          fillMode  = rhi::FillMode::Solid;

    // --- blend ---
    BlendMode           blendMode      = BlendMode::Opaque;
    rhi::ColorWriteMask colorWriteMask = rhi::ColorWriteMask::All;

    // --- depth/stencil ---
    DepthMode            depthMode           = DepthMode::ReadWrite;
    rhi::CompareFunction depthCompare        = rhi::CompareFunction::Less;
    rhi::TextureFormat   depthFormat         = rhi::TextureFormat::Depth32Float;
    i16                  depthBias           = 0;
    f32                  depthBiasSlopeScale = 0.0f;

    // --- render targets ---
    rhi::TextureFormat colorFormats[rhi::maxColorAttachments] = { rhi::TextureFormat::BGRA8Unorm };
    u8                 colorTargetCount = 1;
    u8                 sampleCount      = 1;

    // --- flags ---
    bool depthOnly = false;
    // MRT: write the G-buffer aux targets (normal/velocity, slots 1+). False for transparent/blended
    // draws so they don't clobber the opaque G-buffer they blend over (they still bind all targets to
    // match the render pass, but with the aux write masks disabled).
    bool writeAuxTargets = true;

    // Content hash: name bytes folded with the POD state. Used as the PSO-cache key.
    [[nodiscard]] u64 hashCode() const noexcept {
        u64 h = std::hash<std::u8string_view>{}(shaderName);
        const auto mix = [&h](u64 v) { h = h * 31u + v; };
        mix(static_cast<u64>(shaderFlags));
        mix(static_cast<u64>(vertexLayout));
        mix(customVertexStride);
        mix(customAttributeCount);
        mix(instanced ? 1u : 0u);
        mix(static_cast<u64>(topology));
        mix(static_cast<u64>(cullMode));
        mix(static_cast<u64>(frontFace));
        mix(static_cast<u64>(fillMode));
        mix(static_cast<u64>(blendMode));
        mix(static_cast<u64>(colorWriteMask));
        mix(static_cast<u64>(depthMode));
        mix(static_cast<u64>(depthCompare));
        mix(static_cast<u64>(depthFormat));
        mix(static_cast<u64>(static_cast<u16>(depthBias)));
        for (u8 i = 0; i < colorTargetCount && i < rhi::maxColorAttachments; ++i) {
            mix(static_cast<u64>(colorFormats[i]));
        }
        mix(colorTargetCount);
        mix(sampleCount);
        mix(depthOnly ? 1u : 0u);
        mix(writeAuxTargets ? 1u : 0u);
        return h;
    }

    [[nodiscard]] bool operator==(const PipelineConfig& o) const noexcept {
        if (!(shaderName == o.shaderName && shaderFlags == o.shaderFlags &&
              vertexLayout == o.vertexLayout && customVertexStride == o.customVertexStride &&
              customAttributeCount == o.customAttributeCount && instanced == o.instanced && topology == o.topology &&
              cullMode == o.cullMode && frontFace == o.frontFace && fillMode == o.fillMode &&
              blendMode == o.blendMode && colorWriteMask == o.colorWriteMask &&
              depthMode == o.depthMode && depthCompare == o.depthCompare &&
              depthFormat == o.depthFormat && depthBias == o.depthBias &&
              depthBiasSlopeScale == o.depthBiasSlopeScale &&
              colorTargetCount == o.colorTargetCount && sampleCount == o.sampleCount &&
              depthOnly == o.depthOnly && writeAuxTargets == o.writeAuxTargets)) {
            return false;
        }
        for (u8 i = 0; i < colorTargetCount && i < rhi::maxColorAttachments; ++i) {
            if (colorFormats[i] != o.colorFormats[i]) { return false; }
        }
        return true;
    }

    // ---- presets ----
    [[nodiscard]] static PipelineConfig forOpaqueMesh(std::u8string_view shader, shaders::ShaderFlags flags = shaders::ShaderFlags::None) {
        PipelineConfig c{}; c.shaderName = shader; c.shaderFlags = flags;
        c.vertexLayout = VertexLayoutType::Mesh; c.blendMode = BlendMode::Opaque; c.depthMode = DepthMode::ReadWrite;
        return c;
    }
    [[nodiscard]] static PipelineConfig forTransparentMesh(std::u8string_view shader, shaders::ShaderFlags flags = shaders::ShaderFlags::None) {
        PipelineConfig c{}; c.shaderName = shader; c.shaderFlags = flags;
        c.vertexLayout = VertexLayoutType::Mesh; c.blendMode = BlendMode::AlphaBlend; c.depthMode = DepthMode::ReadOnly;
        return c;
    }
    [[nodiscard]] static PipelineConfig forSkybox(std::u8string_view shader) {
        PipelineConfig c{}; c.shaderName = shader; c.vertexLayout = VertexLayoutType::PositionOnly;
        c.depthMode = DepthMode::ReadOnly; c.depthCompare = rhi::CompareFunction::LessEqual; c.cullMode = CullModeConfig::Front;
        return c;
    }
    [[nodiscard]] static PipelineConfig forSprites(std::u8string_view shader) {
        PipelineConfig c{}; c.shaderName = shader; c.vertexLayout = VertexLayoutType::PositionUVColor;
        c.blendMode = BlendMode::AlphaBlend; c.depthMode = DepthMode::ReadOnly; c.cullMode = CullModeConfig::None;
        return c;
    }
    [[nodiscard]] static PipelineConfig forFullscreen(std::u8string_view shader) {
        PipelineConfig c{}; c.shaderName = shader; c.vertexLayout = VertexLayoutType::None;
        c.depthMode = DepthMode::Disabled; c.cullMode = CullModeConfig::None;
        return c;
    }
};

// Maps VertexLayoutType -> concrete RHI vertex attributes + stride.
class VertexLayoutHelper {
public:
    [[nodiscard]] static u32 stride(VertexLayoutType t) noexcept {
        switch (t) {
        case VertexLayoutType::None:            return 0;
        case VertexLayoutType::PositionOnly:    return 12;
        case VertexLayoutType::PositionUVColor: return 36;
        case VertexLayoutType::MeshNoTangent:   return 32;
        case VertexLayoutType::Mesh:            return 48;
        // A skinned mesh's buffer 0 IS the static stream (48B) - the skinning data
        // is a SEPARATE buffer (see SkinningStream*), matching draco.geometry's
        // SkinnedMesh : StaticMesh layout. So a skinned draw binds two vertex buffers.
        case VertexLayoutType::SkinnedMesh:     return 48;
        case VertexLayoutType::Custom:          return 0;
        }
        return 0;
    }

    [[nodiscard]] static std::span<const rhi::VertexAttribute> attributes(VertexLayoutType t) noexcept {
        switch (t) {
        case VertexLayoutType::PositionOnly:    return { kPositionOnly,    1 };
        case VertexLayoutType::PositionUVColor: return { kPositionUVColor, 3 };
        case VertexLayoutType::MeshNoTangent:   return { kMeshNoTangent,   3 };
        case VertexLayoutType::Mesh:            return { kMesh,            5 };
        case VertexLayoutType::SkinnedMesh:     return { kMesh,            5 };   // buffer 0 = static stream
        default:                                return {};
        }
    }

    [[nodiscard]] static rhi::VertexBufferLayout bufferLayout(VertexLayoutType t) noexcept {
        rhi::VertexBufferLayout l{};
        l.stride = stride(t);
        l.stepMode = rhi::VertexStepMode::Vertex;
        l.attributes = attributes(t);
        return l;
    }

    // The instance-stepped vertex buffer an instanced draw binds: a uint4 DataOffsets entry
    // per instance (location 5), stride 16. `.x` indexes the per-instance StructuredBuffer;
    // hardware instance stepping makes this portable (unlike the SV_InstanceID system value,
    // which differs between DX12 and Vulkan). Bound as buffer 1 alongside the mesh stream.
    [[nodiscard]] static rhi::VertexBufferLayout instanceOffsetsBufferLayout() noexcept {
        rhi::VertexBufferLayout l{};
        l.stride = 16;
        l.stepMode = rhi::VertexStepMode::Instance;
        l.attributes = { kInstanceOffsets, 1 };
        return l;
    }

    // The skinning vertex buffer a skinned draw binds: joints (location 6) + weights (location 7),
    // stride 24 - matches draco.geometry::VertexSkinning. Skinned draws are ALWAYS instanced now, so
    // the instance-stepped DataOffsets takes location 5 and DXC (which assigns input locations
    // sequentially by declaration order, dataOffsets declared before joints/weights) lands these at 6/7.
    [[nodiscard]] static u32 skinningStreamStride() noexcept { return 24; }
    [[nodiscard]] static std::span<const rhi::VertexAttribute> skinningStreamAttributes() noexcept {
        return { kSkinningStream, 2 };
    }
    [[nodiscard]] static rhi::VertexBufferLayout skinningStreamBufferLayout() noexcept {
        rhi::VertexBufferLayout l{};
        l.stride = skinningStreamStride();
        l.stepMode = rhi::VertexStepMode::Vertex;
        l.attributes = skinningStreamAttributes();
        return l;
    }

private:
    using VA = rhi::VertexAttribute;
    using VF = rhi::VertexFormat;
    static constexpr VA kPositionOnly[1]    = { { VF::Float32x3, 0, 0 } };
    static constexpr VA kPositionUVColor[3] = { { VF::Float32x3, 0, 0 }, { VF::Float32x2, 12, 1 }, { VF::Float32x4, 20, 2 } };
    static constexpr VA kMeshNoTangent[3]   = { { VF::Float32x3, 0, 0 }, { VF::Float32x3, 12, 1 }, { VF::Float32x2, 24, 2 } };
    static constexpr VA kMesh[5]            = { { VF::Float32x3, 0, 0 }, { VF::Float32x3, 12, 1 }, { VF::Float32x2, 24, 2 },
                                                { VF::Unorm8x4, 32, 3 }, { VF::Float32x3, 36, 4 } };
    // Skinning stream: joints (uint16x4 packed as uint32x2) + weights, at locations 6/7 (DataOffsets
    // takes 5; skinned draws are always instanced).
    static constexpr VA kSkinningStream[2]  = { { VF::Uint32x2, 0, 6 }, { VF::Float32x4, 8, 7 } };
    // Instance offsets stream: a uint4 DataOffsets at location 5.
    static constexpr VA kInstanceOffsets[1] = { { VF::Uint32x4, 0, 5 } };
};

} // namespace draco::materials
