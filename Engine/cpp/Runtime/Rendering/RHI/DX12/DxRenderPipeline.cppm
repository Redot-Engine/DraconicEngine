/// DX12 implementation of RenderPipeline.
/// Wraps a D3D12 graphics pipeline state object.

module;

#include "DxIncludes.h"
#include <vector>
#include <span>

#include <algorithm>
#include <cstring>

export module rhi.dx12:render_pipeline;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :pipeline_layout;
import :shader_module;
import :pipeline_cache;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxRenderPipelineImpl : public RenderPipeline {
public:
    Status init(ID3D12Device* device, const RenderPipelineDesc& d) {
        m_layout = static_cast<DxPipelineLayoutImpl*>(d.layout);
        if (!m_layout) return ErrorCode::Unknown;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = m_layout->handle();

        // Vertex shader.
        auto* vsMod = static_cast<DxShaderModuleImpl*>(d.vertex.shader.module);
        if (!vsMod) return ErrorCode::Unknown;
        auto vsCode = vsMod->bytecode();
        pso.VS = { vsCode.data(), vsCode.size() };

        // Fragment shader.
        if (d.fragment.has_value()) {
            auto* psMod = static_cast<DxShaderModuleImpl*>(d.fragment->shader.module);
            if (psMod) { auto ps = psMod->bytecode(); pso.PS = { ps.data(), ps.size() }; }
        }

        // Input layout.
        std::vector<D3D12_INPUT_ELEMENT_DESC> elems;
        auto bufs = d.vertex.buffers;
        m_vtxBufCount = static_cast<u32>(std::min(bufs.size(), usize(8)));
        for (usize i = 0; i < bufs.size(); ++i) {
            const auto& buf = bufs[i];
            if (i < 8) m_vtxStrides[i] = buf.stride;
            auto attrs = buf.attributes;
            for (usize j = 0; j < attrs.size(); ++j) {
                const auto& a = attrs[j];
                D3D12_INPUT_ELEMENT_DESC e{};
                e.SemanticName  = "TEXCOORD";
                e.SemanticIndex = a.shaderLocation;
                e.Format        = toDxgiVertexFormat(a.format);
                e.InputSlot     = static_cast<UINT>(i);
                e.AlignedByteOffset = a.offset;
                e.InputSlotClass = (buf.stepMode == VertexStepMode::Instance)
                    ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                    : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                e.InstanceDataStepRate = (buf.stepMode == VertexStepMode::Instance) ? 1 : 0;
                elems.push_back(e);
            }
        }
        pso.InputLayout = { elems.data(), static_cast<UINT>(elems.size()) };

        // Topology.
        pso.PrimitiveTopologyType = toPrimitiveTopologyType(d.primitive.topology);
        m_topology = toPrimitiveTopology(d.primitive.topology);

        // Rasterizer.
        pso.RasterizerState.FillMode = toFillMode(d.primitive.fillMode);
        pso.RasterizerState.CullMode = toCullMode(d.primitive.cullMode);
        pso.RasterizerState.FrontCounterClockwise = (d.primitive.frontFace == FrontFace::CCW) ? TRUE : FALSE;
        pso.RasterizerState.DepthClipEnable = d.primitive.depthClipEnabled ? TRUE : FALSE;
        pso.RasterizerState.MultisampleEnable = (d.multisample.count > 1) ? TRUE : FALSE;

        if (d.depthStencil.has_value()) {
            pso.RasterizerState.DepthBias = d.depthStencil->depthBias;
            pso.RasterizerState.DepthBiasClamp = d.depthStencil->depthBiasClamp;
            pso.RasterizerState.SlopeScaledDepthBias = d.depthStencil->depthBiasSlopeScale;
        }

        // Blend.
        std::span<const ColorTargetState> targets = d.fragment.has_value() ? d.fragment->targets : std::span<const ColorTargetState>();
        pso.BlendState.AlphaToCoverageEnable = d.multisample.alphaToCoverageEnabled ? TRUE : FALSE;
        pso.BlendState.IndependentBlendEnable = (targets.size() > 1) ? TRUE : FALSE;
        for (usize i = 0; i < targets.size() && i < 8; ++i) {
            const auto& t = targets[i];
            auto& rt = pso.BlendState.RenderTarget[i];
            rt.RenderTargetWriteMask = static_cast<UINT8>(t.writeMask);
            if (t.blend.has_value()) {
                rt.BlendEnable = TRUE;
                rt.SrcBlend      = toBlendFactor(t.blend->color.srcFactor);
                rt.DestBlend     = toBlendFactor(t.blend->color.dstFactor);
                rt.BlendOp       = toBlendOp(t.blend->color.operation);
                rt.SrcBlendAlpha = toBlendFactor(t.blend->alpha.srcFactor);
                rt.DestBlendAlpha= toBlendFactor(t.blend->alpha.dstFactor);
                rt.BlendOpAlpha  = toBlendOp(t.blend->alpha.operation);
            }
        }

        // Depth/stencil.
        if (d.depthStencil.has_value()) {
            const auto& ds = *d.depthStencil;
            pso.DepthStencilState.DepthEnable    = ds.depthTestEnabled ? TRUE : FALSE;
            pso.DepthStencilState.DepthWriteMask  = ds.depthWriteEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
            pso.DepthStencilState.DepthFunc       = toComparisonFunc(ds.depthCompare);
            pso.DepthStencilState.StencilEnable   = ds.stencilEnabled ? TRUE : FALSE;
            pso.DepthStencilState.StencilReadMask = ds.stencilReadMask;
            pso.DepthStencilState.StencilWriteMask= ds.stencilWriteMask;

            auto& ff = pso.DepthStencilState.FrontFace;
            ff.StencilFailOp      = toStencilOp(ds.stencilFront.failOp);
            ff.StencilDepthFailOp = toStencilOp(ds.stencilFront.depthFailOp);
            ff.StencilPassOp      = toStencilOp(ds.stencilFront.passOp);
            ff.StencilFunc        = toComparisonFunc(ds.stencilFront.compare);

            auto& bf = pso.DepthStencilState.BackFace;
            bf.StencilFailOp      = toStencilOp(ds.stencilBack.failOp);
            bf.StencilDepthFailOp = toStencilOp(ds.stencilBack.depthFailOp);
            bf.StencilPassOp      = toStencilOp(ds.stencilBack.passOp);
            bf.StencilFunc        = toComparisonFunc(ds.stencilBack.compare);

            pso.DSVFormat = toDxgiFormat(ds.format);
        }

        // Render targets.
        pso.NumRenderTargets = static_cast<UINT>(std::min(targets.size(), usize(8)));
        for (usize i = 0; i < targets.size() && i < 8; ++i)
            pso.RTVFormats[i] = toDxgiFormat(targets[i].format);

        // Multisample.
        pso.SampleDesc.Count = std::max(d.multisample.count, 1u);
        pso.SampleMask = (d.multisample.mask != 0) ? d.multisample.mask : ~0u;

        // Create PSO.
        HRESULT hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pipelineState));
        if (FAILED(hr)) {
            logErrorf("DxRenderPipeline: CreateGraphicsPipelineState failed (0x%08X)", static_cast<unsigned>(hr));
            return ErrorCode::Unknown;
        }
        return ErrorCode::Ok;
    }

    void cleanup() { m_pipelineState.Reset(); }

    [[nodiscard]] ID3D12PipelineState*    handle()   const { return m_pipelineState.Get(); }
    [[nodiscard]] D3D_PRIMITIVE_TOPOLOGY  topology() const { return m_topology; }
    [[nodiscard]] DxPipelineLayoutImpl*   pipelineLayout() const { return m_layout; }
    [[nodiscard]] u32 getVertexStride(u32 slot) const { return (slot < m_vtxBufCount) ? m_vtxStrides[slot] : 0; }

private:
    ComPtr<ID3D12PipelineState>  m_pipelineState;
    DxPipelineLayoutImpl*        m_layout      = nullptr;
    D3D_PRIMITIVE_TOPOLOGY       m_topology    = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    u32                          m_vtxStrides[8]{};
    u32                          m_vtxBufCount = 0;
};

} // namespace draco::rhi::dx12
