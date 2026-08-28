/// DX12 implementation of Buffer.

module;

#include "DxIncludes.h"

export module rhi.dx12:buffer;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxDeviceImpl; // forward

class DxBufferImpl : public Buffer {
public:
    Status init(ID3D12Device* device, const BufferDesc& d) {
        desc = d;

        auto heapType = toHeapType(d.memory);
        auto flags    = toBufferResourceFlags(d.usage);

        // Initial state based on heap type.
        m_state = D3D12_RESOURCE_STATE_COMMON;
        if (heapType == D3D12_HEAP_TYPE_UPLOAD)   m_state = D3D12_RESOURCE_STATE_GENERIC_READ;
        if (heapType == D3D12_HEAP_TYPE_READBACK)  m_state = D3D12_RESOURCE_STATE_COPY_DEST;

        u64 alignedSize = (d.size + 255) & ~u64(255); // 256-byte alignment for CBVs

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = heapType;

        D3D12_RESOURCE_DESC rd{};
        rd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width            = alignedSize;
        rd.Height           = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels        = 1;
        rd.Format           = DXGI_FORMAT_UNKNOWN;
        rd.SampleDesc       = { 1, 0 };
        rd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        rd.Flags            = flags;

        HRESULT hr = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE,
            &rd, m_state, nullptr,
            IID_PPV_ARGS(&m_resource));
        if (FAILED(hr)) {
            logErrorf("DxBuffer: CreateCommittedResource failed (0x%08X)", static_cast<unsigned>(hr));
            return ErrorCode::Unknown;
        }

        // Persistently map upload/readback buffers.
        if (heapType == D3D12_HEAP_TYPE_UPLOAD || heapType == D3D12_HEAP_TYPE_READBACK)
            m_resource->Map(0, nullptr, &m_persistentMap);

        return ErrorCode::Ok;
    }

    void* map() override {
        if (m_persistentMap) return m_persistentMap;
        void* ptr = nullptr;
        if (SUCCEEDED(m_resource->Map(0, nullptr, &ptr))) return ptr;
        return nullptr;
    }

    void unmap() override {
        if (m_persistentMap) return; // don't unmap persistently mapped buffers
        m_resource->Unmap(0, nullptr);
    }

    void cleanup() {
        if (m_persistentMap) { m_resource->Unmap(0, nullptr); m_persistentMap = nullptr; }
        m_resource.Reset();
    }

    // ---- Internal ----
    [[nodiscard]] ID3D12Resource*       handle() const { return m_resource.Get(); }
    [[nodiscard]] D3D12_RESOURCE_STATES currentState() const { return m_state; }
    void setState(D3D12_RESOURCE_STATES s) { m_state = s; }
    [[nodiscard]] D3D12_GPU_VIRTUAL_ADDRESS gpuAddress() const { return m_resource ? m_resource->GetGPUVirtualAddress() : 0; }

private:
    ComPtr<ID3D12Resource>  m_resource;
    D3D12_RESOURCE_STATES   m_state = D3D12_RESOURCE_STATE_COMMON;
    void*                   m_persistentMap = nullptr;
};

} // namespace draco::rhi::dx12
