/// DX12 implementation of CommandPool.
/// Wraps an ID3D12CommandAllocator.

module;

#include "DxIncludes.h"
#include <vector>

export module rhi.dx12:command_pool;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :command_buffer;
import :descriptor_staging;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxDeviceImpl;          // forward
class DxCommandEncoderImpl;  // forward

class DxCommandPoolImpl : public CommandPool {
public:
    Status init(DxDeviceImpl* device, ID3D12Device* d3dDevice, QueueType queueType,
                DxGpuDescriptorHeap* cpuSrvHeap, DxGpuDescriptorHeap* gpuSrvHeap,
                DxGpuDescriptorHeap* cpuSamplerHeap, DxGpuDescriptorHeap* gpuSamplerHeap) {
        m_device    = device;
        m_d3dDevice = d3dDevice;
        m_type      = toCommandListType(queueType);

        HRESULT hr = d3dDevice->CreateCommandAllocator(m_type, IID_PPV_ARGS(&m_allocator));
        if (FAILED(hr)) return ErrorCode::Unknown;

        // Create descriptor staging (shared by all encoders from this pool).
        m_srvStaging.init(cpuSrvHeap, gpuSrvHeap, d3dDevice,
                         D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024);
        m_samplerStaging.init(cpuSamplerHeap, gpuSamplerHeap, d3dDevice,
                             D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 64);

        return ErrorCode::Ok;
    }

    // ---- CommandPool interface ----
    Status createEncoder(CommandEncoder*& out) override;
    void   destroyEncoder(CommandEncoder*& encoder) override;

    void reset() override {
        releaseCommandBuffers();
        // Reset descriptor staging -- GPU is done (fence waited), so staging
        // bump pointers can safely return to start.
        m_srvStaging.reset();
        m_samplerStaging.reset();
        m_allocator->Reset();
    }

    void cleanup() {
        releaseCommandBuffers();
        m_srvStaging.destroy();
        m_samplerStaging.destroy();
        m_allocator.Reset();
    }

    // ---- Internal ----
    [[nodiscard]] ID3D12CommandAllocator* handle()          const { return m_allocator.Get(); }
    [[nodiscard]] DxDeviceImpl*           ownerDevice()     const { return m_device; }
    [[nodiscard]] DxDescriptorStaging*    srvStaging()            { return &m_srvStaging; }
    [[nodiscard]] DxDescriptorStaging*    samplerStaging()        { return &m_samplerStaging; }

    /// Called by DxCommandEncoderImpl::finish() to register a command buffer with this pool.
    void trackCommandBuffer(DxCommandBufferImpl* cb) { m_trackedBuffers.push_back(cb); }

private:
    void releaseCommandBuffers() {
        for (auto* cb : m_trackedBuffers) {
            cb->release();
            delete cb;
        }
        m_trackedBuffers.clear();
    }

    ComPtr<ID3D12CommandAllocator>      m_allocator;
    ID3D12Device*                       m_d3dDevice = nullptr;
    DxDeviceImpl*                       m_device     = nullptr;
    D3D12_COMMAND_LIST_TYPE             m_type       = D3D12_COMMAND_LIST_TYPE_DIRECT;
    std::vector<DxCommandBufferImpl*>          m_trackedBuffers;
    DxDescriptorStaging                 m_srvStaging;
    DxDescriptorStaging                 m_samplerStaging;
};

} // namespace draco::rhi::dx12
