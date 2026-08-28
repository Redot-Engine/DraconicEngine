/// DX12 implementation of BindGroup.
/// Allocates contiguous descriptor ranges in CPU-visible heaps and writes descriptors.

module;

#include "DxIncludes.h"
#include <vector>
#include <span>

export module rhi.dx12:bind_group;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :bind_group_layout;
import :gpu_descriptor_heap;
import :buffer;
import :texture;
import :texture_view;
import :sampler;
import :accel_struct;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxAccelStructImpl; // forward

class DxBindGroupImpl : public BindGroup {
public:
    Status init(ID3D12Device* device, const BindGroupDesc& d,
                DxGpuDescriptorHeap* cpuSrvHeap, DxGpuDescriptorHeap* cpuSamplerHeap) {
        m_device         = device;
        m_layout         = static_cast<DxBindGroupLayoutImpl*>(d.layout);
        m_cpuSrvHeap     = cpuSrvHeap;
        m_cpuSamplerHeap = cpuSamplerHeap;
        if (!m_layout) return ErrorCode::Unknown;

        // Cache counts so cleanup doesn't need to access m_layout (which may be destroyed first).
        m_cachedCbvSrvUavCount = m_layout->cbvSrvUavCount();
        m_cachedSamplerCount   = m_layout->samplerCount();

        if (m_cachedCbvSrvUavCount > 0) {
            m_cbvSrvUavOffset = cpuSrvHeap->allocate(m_cachedCbvSrvUavCount);
            if (m_cbvSrvUavOffset < 0) return ErrorCode::Unknown;
        }
        if (m_cachedSamplerCount > 0) {
            m_samplerOffset = cpuSamplerHeap->allocate(m_cachedSamplerCount);
            if (m_samplerOffset < 0) return ErrorCode::Unknown;
        }

        writeDescriptors(d);
        return ErrorCode::Ok;
    }

    BindGroupLayout* layout() override { return m_layout; }

    void updateBindless(std::span<const BindlessUpdateEntry> entries) override {
        auto ranges = m_layout->ranges();
        for (usize i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            if (e.layoutIndex >= static_cast<u32>(ranges.size())) continue;
            const auto& r = ranges[e.layoutIndex];

            BindGroupEntry bgEntry{};
            bgEntry.buffer      = e.buffer;
            bgEntry.bufferOffset= e.bufferOffset;
            bgEntry.bufferSize  = e.bufferSize;
            bgEntry.textureView = e.textureView;
            bgEntry.sampler     = e.sampler;

            if (r.isSampler)
                writeSampler(bgEntry, r, e.arrayIndex);
            else
                writeCbvSrvUav(bgEntry, r, e.arrayIndex);
        }
    }

    void cleanup() {
        if (m_cbvSrvUavOffset >= 0 && m_cachedCbvSrvUavCount > 0)
            m_cpuSrvHeap->free(static_cast<u32>(m_cbvSrvUavOffset), m_cachedCbvSrvUavCount);
        if (m_samplerOffset >= 0 && m_cachedSamplerCount > 0)
            m_cpuSamplerHeap->free(static_cast<u32>(m_samplerOffset), m_cachedSamplerCount);
        m_cbvSrvUavOffset = -1; m_samplerOffset = -1;
    }

    [[nodiscard]] i32 cbvSrvUavOffset()  const { return m_cbvSrvUavOffset; }
    [[nodiscard]] i32 samplerOffset()    const { return m_samplerOffset; }
    [[nodiscard]] std::span<const u64> dynamicGpuAddresses() const { return { m_dynAddrs.data(), m_dynAddrs.size() }; }

private:
    void writeDescriptors(const BindGroupDesc& d) {
        auto ranges = m_layout->ranges();
        usize entryIdx = 0;
        for (usize i = 0; i < ranges.size(); ++i) {
            const auto& r = ranges[i];
            switch (r.type) {
            case BindingType::BindlessTextures: case BindingType::BindlessSamplers:
            case BindingType::BindlessStorageBuffers: case BindingType::BindlessStorageTextures:
                continue;
            default: break;
            }
            if (entryIdx >= d.entries.size()) break;
            const auto& e = d.entries[entryIdx++];

            if (r.hasDynamicOffset) {
                if (auto* buf = static_cast<DxBufferImpl*>(e.buffer))
                    m_dynAddrs.push_back(buf->gpuAddress() + e.bufferOffset);
                else
                    m_dynAddrs.push_back(0);
                continue;
            }
            if (r.isSampler) writeSampler(e, r);
            else             writeCbvSrvUav(e, r);
        }
    }

    void writeCbvSrvUav(const BindGroupEntry& e, const DxBindingRangeInfo& r, u32 arrayIdx = 0) {
        u32 off = static_cast<u32>(m_cbvSrvUavOffset) + r.heapOffset + arrayIdx;
        D3D12_CPU_DESCRIPTOR_HANDLE dest = m_cpuSrvHeap->getCpuHandle(off);

        switch (r.type) {
        case BindingType::UniformBuffer:
            if (auto* buf = static_cast<DxBufferImpl*>(e.buffer)) {
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbv{};
                cbv.BufferLocation = buf->gpuAddress() + e.bufferOffset;
                u64 sz = (e.bufferSize > 0) ? e.bufferSize : buf->desc.size;
                cbv.SizeInBytes = static_cast<UINT>((sz + 255) & ~u64(255));
                m_device->CreateConstantBufferView(&cbv, dest);
            }
            break;
        case BindingType::StorageBufferReadOnly:
            if (auto* buf = static_cast<DxBufferImpl*>(e.buffer)) {
                u64 sz = (e.bufferSize > 0) ? e.bufferSize : buf->desc.size;
                D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
                srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                if (r.storageBufferStride > 0) {
                    srv.Format = DXGI_FORMAT_UNKNOWN;
                    srv.Buffer.FirstElement = static_cast<UINT64>(e.bufferOffset / r.storageBufferStride);
                    srv.Buffer.NumElements  = static_cast<UINT>(sz / r.storageBufferStride);
                    srv.Buffer.StructureByteStride = r.storageBufferStride;
                } else {
                    srv.Format = DXGI_FORMAT_R32_TYPELESS;
                    srv.Buffer.FirstElement = static_cast<UINT64>(e.bufferOffset / 4);
                    srv.Buffer.NumElements  = static_cast<UINT>(sz / 4);
                    srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                }
                m_device->CreateShaderResourceView(buf->handle(), &srv, dest);
            }
            break;
        case BindingType::StorageBufferReadWrite:
            if (auto* buf = static_cast<DxBufferImpl*>(e.buffer)) {
                u64 sz = (e.bufferSize > 0) ? e.bufferSize : buf->desc.size;
                D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
                uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                if (r.storageBufferStride > 0) {
                    uav.Format = DXGI_FORMAT_UNKNOWN;
                    uav.Buffer.FirstElement = static_cast<UINT64>(e.bufferOffset / r.storageBufferStride);
                    uav.Buffer.NumElements  = static_cast<UINT>(sz / r.storageBufferStride);
                    uav.Buffer.StructureByteStride = r.storageBufferStride;
                } else {
                    uav.Format = DXGI_FORMAT_R32_TYPELESS;
                    uav.Buffer.FirstElement = static_cast<UINT64>(e.bufferOffset / 4);
                    uav.Buffer.NumElements  = static_cast<UINT>(sz / 4);
                    uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
                }
                m_device->CreateUnorderedAccessView(buf->handle(), nullptr, &uav, dest);
            }
            break;
        case BindingType::SampledTexture: case BindingType::BindlessTextures:
            if (auto* v = static_cast<DxTextureViewImpl*>(e.textureView))
                m_device->CopyDescriptorsSimple(1, dest, v->getSrv(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            break;
        case BindingType::StorageTextureReadOnly: case BindingType::StorageTextureReadWrite:
        case BindingType::BindlessStorageTextures:
            if (auto* v = static_cast<DxTextureViewImpl*>(e.textureView))
                m_device->CopyDescriptorsSimple(1, dest, v->getUav(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            break;
        case BindingType::AccelerationStructure:
            if (auto* dxAs = static_cast<DxAccelStructImpl*>(e.accelStruct)) {
                D3D12_SHADER_RESOURCE_VIEW_DESC asSrv{};
                asSrv.Format = DXGI_FORMAT_UNKNOWN;
                asSrv.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
                asSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                asSrv.RaytracingAccelerationStructure.Location = dxAs->deviceAddress();
                m_device->CreateShaderResourceView(nullptr, &asSrv, dest);
            }
            break;
        default: break;
        }
    }

    void writeSampler(const BindGroupEntry& e, const DxBindingRangeInfo& r, u32 arrayIdx = 0) {
        u32 off = static_cast<u32>(m_samplerOffset) + r.heapOffset + arrayIdx;
        D3D12_CPU_DESCRIPTOR_HANDLE dest = m_cpuSamplerHeap->getCpuHandle(off);
        if (auto* s = static_cast<DxSamplerImpl*>(e.sampler))
            m_device->CopyDescriptorsSimple(1, dest, s->handle(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

    ID3D12Device*              m_device         = nullptr;
    DxBindGroupLayoutImpl*     m_layout         = nullptr;
    DxGpuDescriptorHeap*       m_cpuSrvHeap     = nullptr;
    DxGpuDescriptorHeap*       m_cpuSamplerHeap = nullptr;
    u32                        m_cachedCbvSrvUavCount = 0;
    u32                        m_cachedSamplerCount   = 0;
    i32                        m_cbvSrvUavOffset = -1;
    i32                        m_samplerOffset   = -1;
    std::vector<u64>                 m_dynAddrs;
};

} // namespace draco::rhi::dx12
