/// Simple CPU-side descriptor heap allocator with free-list.

module;

#include "DxIncludes.h"
#include <vector>

export module rhi.dx12:descriptor_heap;

import core.stdtypes;
import core.status;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxDescriptorHeapAllocator {
public:
    DxDescriptorHeapAllocator() = default;

    Status init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 maxCount,
                D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE) {
        m_maxCount = maxCount;
        m_alive.resize(maxCount, static_cast<u8>(0));

        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type           = type;
        hd.NumDescriptors = maxCount;
        hd.Flags          = flags;
        HRESULT hr = device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_heap));
        if (FAILED(hr)) return ErrorCode::Unknown;

        m_heapStart      = m_heap->GetCPUDescriptorHandleForHeapStart();
        m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
        return ErrorCode::Ok;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE allocate() {
        for (u32 i = 0; i < m_maxCount; ++i) {
            u32 idx = (m_searchStart + i) % m_maxCount;
            if (!m_alive[idx]) {
                m_alive[idx] = true;
                ++m_allocCount;
                m_searchStart = (idx + 1) % m_maxCount;
                D3D12_CPU_DESCRIPTOR_HANDLE h{};
                h.ptr = m_heapStart.ptr + static_cast<SIZE_T>(idx) * m_descriptorSize;
                return h;
            }
        }
        return {}; // heap full
    }

    void free(D3D12_CPU_DESCRIPTOR_HANDLE handle) {
        if (handle.ptr < m_heapStart.ptr) return;
        u32 offset = static_cast<u32>((handle.ptr - m_heapStart.ptr) / m_descriptorSize);
        if (offset < m_maxCount && m_alive[offset]) {
            m_alive[offset] = false;
            --m_allocCount;
        }
    }

    void destroy() {
        m_heap.Reset();
        m_alive.clear();
    }

    [[nodiscard]] ID3D12DescriptorHeap* heap() const { return m_heap.Get(); }
    [[nodiscard]] u32 descriptorSize() const { return m_descriptorSize; }

private:
    ComPtr<ID3D12DescriptorHeap>  m_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE   m_heapStart{};
    u32                           m_descriptorSize = 0;
    u32                           m_maxCount    = 0;
    u32                           m_allocCount  = 0;
    u32                           m_searchStart = 0;
    std::vector<u8>                     m_alive;
};

} // namespace draco::rhi::dx12
