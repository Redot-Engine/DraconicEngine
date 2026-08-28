/// DX12 implementation of ComputePassEncoder.
/// Records compute dispatch commands into the parent command encoder's command list.

module;

#include "DxIncludes.h"
#include <span>

export module rhi.dx12:compute_pass_encoder;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :buffer;
import :bind_group;
import :bind_group_layout;
import :compute_pipeline;
import :pipeline_layout;
import :query_set;
import :descriptor_staging;
import :gpu_descriptor_heap;

using namespace draco;

export namespace draco::rhi::dx12 {

/// Pointers needed by the compute pass encoder, provided by the command encoder.
struct DxComputePassContext {
    ID3D12GraphicsCommandList* cmdList  = nullptr;
    DxDescriptorStaging*       srvStaging     = nullptr;
    DxDescriptorStaging*       samplerStaging = nullptr;
    DxGpuDescriptorHeap*       gpuSrvHeap     = nullptr;
    DxGpuDescriptorHeap*       gpuSamplerHeap = nullptr;
    // Indirect command signature (cached on device).
    ID3D12CommandSignature*    dispatchSig    = nullptr;
};

class DxComputePassEncoderImpl : public ComputePassEncoder {
public:
    explicit DxComputePassEncoderImpl(const DxComputePassContext& ctx)
        : m_ctx(ctx) {}

    void begin() {
        m_currentPipeline = nullptr;
    }

    // ---- Pipeline & Binding ----

    void setPipeline(ComputePipeline* pipeline) override {
        auto* dxPipeline = static_cast<DxComputePipelineImpl*>(pipeline);
        if (!dxPipeline) return;
        m_currentPipeline = dxPipeline;

        auto* cmdList = m_ctx.cmdList;
        cmdList->SetPipelineState(dxPipeline->handle());
        cmdList->SetComputeRootSignature(dxPipeline->pipelineLayout()->handle());
    }

    void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynamicOffsets) override {
        auto* dxGroup = static_cast<DxBindGroupImpl*>(group);
        if (!dxGroup || !m_currentPipeline) return;

        auto* layout = m_currentPipeline->pipelineLayout();
        if (!layout) return;

        auto* cmdList  = m_ctx.cmdList;
        auto* dxLayout = static_cast<DxBindGroupLayoutImpl*>(dxGroup->layout());

        // Copy-on-bind: copy into encoder's staging region, bind from staging offset.
        if (dxGroup->cbvSrvUavOffset() >= 0 && dxLayout && dxLayout->cbvSrvUavCount() > 0) {
            i32 rootIdx = layout->getCbvSrvUavRootIndex(index);
            if (rootIdx >= 0) {
                i32 stagedOffset = m_ctx.srvStaging->copyFrom(
                    static_cast<u32>(dxGroup->cbvSrvUavOffset()), dxLayout->cbvSrvUavCount());
                if (stagedOffset >= 0) {
                    auto gpuHandle = m_ctx.gpuSrvHeap->getGpuHandle(static_cast<u32>(stagedOffset));
                    cmdList->SetComputeRootDescriptorTable(static_cast<UINT>(rootIdx), gpuHandle);
                }
            }
        }

        if (dxGroup->samplerOffset() >= 0 && dxLayout && dxLayout->samplerCount() > 0) {
            i32 rootIdx = layout->getSamplerRootIndex(index);
            if (rootIdx >= 0) {
                i32 stagedOffset = m_ctx.samplerStaging->copyFrom(
                    static_cast<u32>(dxGroup->samplerOffset()), dxLayout->samplerCount());
                if (stagedOffset >= 0) {
                    auto gpuHandle = m_ctx.gpuSamplerHeap->getGpuHandle(static_cast<u32>(stagedOffset));
                    cmdList->SetComputeRootDescriptorTable(static_cast<UINT>(rootIdx), gpuHandle);
                }
            }
        }

        // Bind dynamic offset root descriptors (not staged -- uses GPU virtual addresses).
        auto dynAddrs = dxGroup->dynamicGpuAddresses();
        usize dynOffsetIdx = 0;
        for (usize i = 0; i < layout->dynamicRootEntries().size(); ++i) {
            const auto& entry = layout->dynamicRootEntries()[i];
            if (entry.groupIndex != index) continue;
            if (entry.dynamicIndex >= static_cast<u32>(dynAddrs.size())) continue;

            u64 gpuAddr = dynAddrs[entry.dynamicIndex];
            if (dynOffsetIdx < dynamicOffsets.size())
                gpuAddr += static_cast<u64>(dynamicOffsets[dynOffsetIdx]);
            ++dynOffsetIdx;

            switch (entry.paramType) {
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
                cmdList->SetComputeRootConstantBufferView(static_cast<UINT>(entry.rootParamIndex), gpuAddr);
                break;
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
                cmdList->SetComputeRootShaderResourceView(static_cast<UINT>(entry.rootParamIndex), gpuAddr);
                break;
            case D3D12_ROOT_PARAMETER_TYPE_UAV:
                cmdList->SetComputeRootUnorderedAccessView(static_cast<UINT>(entry.rootParamIndex), gpuAddr);
                break;
            default: break;
            }
        }
    }

    void setPushConstants(ShaderStage /*stages*/, u32 offset, u32 size, const void* data) override {
        if (!m_currentPipeline) return;
        auto* layout = m_currentPipeline->pipelineLayout();
        if (!layout || layout->pushConstantRootIndex() < 0) return;

        m_ctx.cmdList->SetComputeRoot32BitConstants(
            static_cast<UINT>(layout->pushConstantRootIndex()),
            size / 4, data, offset / 4);
    }

    // ---- Dispatch ----

    void dispatch(u32 x, u32 y, u32 z) override {
        m_ctx.cmdList->Dispatch(x, y, z);
    }

    void dispatchIndirect(Buffer* buffer, u64 offset) override {
        auto* dxBuf = static_cast<DxBufferImpl*>(buffer);
        if (!dxBuf) return;

        auto* sig = m_ctx.dispatchSig;
        if (!sig) return;

        m_ctx.cmdList->ExecuteIndirect(sig, 1, dxBuf->handle(), offset, nullptr, 0);
    }

    // ---- Barrier ----

    void computeBarrier() override {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.UAV.pResource = nullptr; // global UAV barrier
        m_ctx.cmdList->ResourceBarrier(1, &barrier);
    }

    // ---- Queries ----

    void writeTimestamp(QuerySet* querySet, u32 index) override {
        auto* qs = static_cast<DxQuerySetImpl*>(querySet);
        if (qs) m_ctx.cmdList->EndQuery(qs->handle(), D3D12_QUERY_TYPE_TIMESTAMP, index);
    }

    // ---- End ----

    void end() override {
        m_currentPipeline = nullptr;
    }

private:
    DxComputePassContext    m_ctx;
    DxComputePipelineImpl*  m_currentPipeline = nullptr;
};

} // namespace draco::rhi::dx12
