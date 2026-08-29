/// DX12 implementation of MeshPipeline.
/// Uses pipeline state stream (ID3D12Device2::CreatePipelineState) since
/// mesh shader pipelines cannot use the traditional D3D12_GRAPHICS_PIPELINE_STATE_DESC.

module;

#include "DxIncludes.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

export module rhi.dx12:mesh_pipeline;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :pipeline_layout;
import :shader_module;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxMeshPipelineImpl : public MeshPipeline {
public:
    Status init(ID3D12Device* device, const MeshPipelineDesc& desc) {
        m_layout = static_cast<DxPipelineLayoutImpl*>(desc.layout);
        if (!m_layout) {
            logErrorf("DxMeshPipeline: pipeline layout is null");
            return ErrorCode::Unknown;
        }
        layout = desc.layout;

        // Query ID3D12Device2 for CreatePipelineState (pipeline state stream API).
        ComPtr<ID3D12Device2> device2;
        HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&device2));
        if (FAILED(hr) || !device2) {
            logErrorf("DxMeshPipeline: QueryInterface for ID3D12Device2 failed (0x%08X)", static_cast<unsigned>(hr));
            return ErrorCode::Unknown;
        }

        // Build pipeline state stream as raw bytes.
        // Subobjects packed sequentially: root sig, MS, [AS], [PS], blend,
        // sample mask, rasterizer, [depth/stencil], [DS format], RT formats, sample desc.
        alignas(8) u8 streamBuffer[2048]{};
        usize offset = 0;

        // Root signature (manual - pointer type cannot use writeSubobject<T>).
        {
            offset = (offset + 7) & ~usize(7);
            std::memcpy(&streamBuffer[offset], &(const D3D12_PIPELINE_STATE_SUBOBJECT_TYPE&)
                (D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE), sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE));
            offset += sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE);
            offset = (offset + 7) & ~usize(7); // pointer-align
            auto* rootSig = m_layout->handle();
            std::memcpy(&streamBuffer[offset], &rootSig, sizeof(rootSig));
            offset += sizeof(rootSig);
        }

        // Mesh shader (required).
        auto* msMod = static_cast<DxShaderModuleImpl*>(desc.mesh.module);
        if (!msMod) {
            logErrorf("DxMeshPipeline: mesh shader module is null");
            return ErrorCode::Unknown;
        }
        {
            auto msCode = msMod->bytecode();
            D3D12_SHADER_BYTECODE msBC{ msCode.data(), msCode.size() };
            writeSubobject(streamBuffer, offset, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, msBC);
        }

        // Task/amplification shader (optional).
        if (desc.task.has_value()) {
            if (auto* asMod = static_cast<DxShaderModuleImpl*>(desc.task->module)) {
                auto asCode = asMod->bytecode();
                D3D12_SHADER_BYTECODE asBC{ asCode.data(), asCode.size() };
                writeSubobject(streamBuffer, offset, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS, asBC);
            }
        }

        // Fragment/pixel shader (optional).
        if (desc.fragment.has_value()) {
            if (auto* psMod = static_cast<DxShaderModuleImpl*>(desc.fragment->shader.module)) {
                auto psCode = psMod->bytecode();
                D3D12_SHADER_BYTECODE psBC{ psCode.data(), psCode.size() };
                writeSubobject(streamBuffer, offset, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, psBC);
            }
        }

        // Blend state.
        auto colorTargets = desc.colorTargets;
        D3D12_BLEND_DESC blendDesc{};
        blendDesc.AlphaToCoverageEnable = desc.multisample.alphaToCoverageEnabled ? TRUE : FALSE;
        blendDesc.IndependentBlendEnable = (colorTargets.size() > 1) ? TRUE : FALSE;
        for (usize i = 0; i < colorTargets.size() && i < 8; ++i) {
            const auto& t = colorTargets[i];
            auto& rt = blendDesc.RenderTarget[i];
            rt.RenderTargetWriteMask = static_cast<UINT8>(t.writeMask);
            if (t.blend.has_value()) {
                rt.BlendEnable    = TRUE;
                rt.SrcBlend       = toBlendFactor(t.blend->color.srcFactor);
                rt.DestBlend      = toBlendFactor(t.blend->color.dstFactor);
                rt.BlendOp        = toBlendOp(t.blend->color.operation);
                rt.SrcBlendAlpha  = toBlendFactor(t.blend->alpha.srcFactor);
                rt.DestBlendAlpha = toBlendFactor(t.blend->alpha.dstFactor);
                rt.BlendOpAlpha   = toBlendOp(t.blend->alpha.operation);
            }
        }
        writeSubobject(streamBuffer, offset, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND, blendDesc);

        // Sample mask.
        UINT sampleMask = (desc.multisample.mask != 0) ? desc.multisample.mask : ~0u;
        writeSubobject(streamBuffer, offset, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK, sampleMask);

        // Rasterizer state.
        D3D12_RASTERIZER_DESC rasterDesc{};
        rasterDesc.FillMode = toFillMode(desc.primitive.fillMode);
        rasterDesc.CullMode = toCullMode(desc.primitive.cullMode);
        rasterDesc.FrontCounterClockwise = (desc.primitive.frontFace == FrontFace::CCW) ? TRUE : FALSE;
        rasterDesc.DepthClipEnable = desc.primitive.depthClipEnabled ? TRUE : FALSE;
        rasterDesc.MultisampleEnable = (desc.multisample.count > 1) ? TRUE : FALSE;

        if (desc.depthStencil.has_value()) {
            rasterDesc.DepthBias            = desc.depthStencil->depthBias;
            rasterDesc.DepthBiasClamp       = desc.depthStencil->depthBiasClamp;
            rasterDesc.SlopeScaledDepthBias = desc.depthStencil->depthBiasSlopeScale;
        }

        writeSubobject(streamBuffer, offset, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, rasterDesc);

        // Depth/stencil state.
        if (desc.depthStencil.has_value()) {
            const auto& ds = *desc.depthStencil;

            D3D12_DEPTH_STENCIL_DESC dsDesc{};
            dsDesc.DepthEnable    = ds.depthTestEnabled  ? TRUE : FALSE;
            dsDesc.DepthWriteMask = ds.depthWriteEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
            dsDesc.DepthFunc      = toComparisonFunc(ds.depthCompare);
            dsDesc.StencilEnable  = ds.stencilEnabled    ? TRUE : FALSE;
            dsDesc.StencilReadMask  = ds.stencilReadMask;
            dsDesc.StencilWriteMask = ds.stencilWriteMask;

            auto& ff = dsDesc.FrontFace;
            ff.StencilFailOp      = toStencilOp(ds.stencilFront.failOp);
            ff.StencilDepthFailOp = toStencilOp(ds.stencilFront.depthFailOp);
            ff.StencilPassOp      = toStencilOp(ds.stencilFront.passOp);
            ff.StencilFunc        = toComparisonFunc(ds.stencilFront.compare);

            auto& bf = dsDesc.BackFace;
            bf.StencilFailOp      = toStencilOp(ds.stencilBack.failOp);
            bf.StencilDepthFailOp = toStencilOp(ds.stencilBack.depthFailOp);
            bf.StencilPassOp      = toStencilOp(ds.stencilBack.passOp);
            bf.StencilFunc        = toComparisonFunc(ds.stencilBack.compare);

            writeSubobject(streamBuffer, offset, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL, dsDesc);

            // Depth/stencil format.
            DXGI_FORMAT dsFormat = toDxgiFormat(ds.format);
            writeSubobject(streamBuffer, offset, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT, dsFormat);
        }

        // Render target formats.
        D3D12_RT_FORMAT_ARRAY rtFormats{};
        rtFormats.NumRenderTargets = static_cast<UINT>(std::min(colorTargets.size(), usize(8)));
        for (usize i = 0; i < colorTargets.size() && i < 8; ++i)
            rtFormats.RTFormats[i] = toDxgiFormat(colorTargets[i].format);
        writeSubobject(streamBuffer, offset, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, rtFormats);

        // Sample desc.
        DXGI_SAMPLE_DESC sampleDesc{};
        sampleDesc.Count   = std::max(desc.multisample.count, 1u);
        sampleDesc.Quality = 0;
        writeSubobject(streamBuffer, offset, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC, sampleDesc);

        // Create pipeline state via stream API.
        D3D12_PIPELINE_STATE_STREAM_DESC streamDesc{};
        streamDesc.SizeInBytes                   = static_cast<SIZE_T>(offset);
        streamDesc.pPipelineStateSubobjectStream = streamBuffer;

        hr = device2->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&m_pipelineState));
        if (FAILED(hr)) {
            logErrorf("DxMeshPipeline: CreatePipelineState failed (0x%08X)", static_cast<unsigned>(hr));
            return ErrorCode::Unknown;
        }
        return ErrorCode::Ok;
    }

    void cleanup() { m_pipelineState.Reset(); }

    [[nodiscard]] ID3D12PipelineState*  handle()         const { return m_pipelineState.Get(); }
    [[nodiscard]] DxPipelineLayoutImpl* pipelineLayout() const { return m_layout; }

private:
    /// Writes a pipeline state stream subobject into the buffer.
    /// Each subobject is: { type (aligned to 8), padding to value alignment, value }.
    template<typename T>
    static void writeSubobject(u8* buffer, usize& offset,
                               D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type, const T& value) {
        // Align subobject start to pointer size (8 bytes on 64-bit).
        offset = (offset + 7) & ~usize(7);

        // Write type.
        std::memcpy(&buffer[offset], &type, sizeof(type));
        offset += sizeof(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE);

        // Pad to natural alignment of value within the subobject.
        constexpr usize valueAlign = alignof(T);
        offset = (offset + valueAlign - 1) & ~(valueAlign - 1);

        // Write value.
        std::memcpy(&buffer[offset], &value, sizeof(T));
        offset += sizeof(T);
    }

    ComPtr<ID3D12PipelineState>  m_pipelineState;
    DxPipelineLayoutImpl*        m_layout = nullptr;
};

} // namespace draco::rhi::dx12
