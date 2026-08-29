/// GPU-visible descriptor heap with contiguous block allocation.
/// Used for CBV/SRV/UAV and Sampler heaps that are shader-visible.

module;

#include "DxIncludes.h"
#include <vector>

export module rhi.dx12:gpu_descriptor_heap;

import core.stdtypes;
import core.status;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxGpuDescriptorHeap {
public:
    DxGpuDescriptorHeap() = default;

    Status init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 capacity,
                bool shaderVisible = true) {
        m_capacity = capacity;

        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type           = type;
        hd.NumDescriptors = capacity;
        hd.Flags          = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                          : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HRESULT hr = device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&m_heap));
        if (FAILED(hr)) return ErrorCode::Unknown;

        m_cpuStart      = m_heap->GetCPUDescriptorHandleForHeapStart();
        if (shaderVisible)
            m_gpuStart  = m_heap->GetGPUDescriptorHandleForHeapStart();
        m_incrementSize = device->GetDescriptorHandleIncrementSize(type);
        return ErrorCode::Ok;
    }

    /// Allocate a contiguous block. Returns offset or -1.
    i32 allocate(u32 count) {
        if (count == 0) return -1;
        // First-fit from free list.
        for (usize i = 0; i < m_freeBlocks.size(); ++i) {
            auto& b = m_freeBlocks[i];
            if (b.count >= count) {
                u32 off = b.offset;
                if (b.count == count)
                    m_freeBlocks.erase(m_freeBlocks.begin() + i);
                else
                    b = { b.offset + count, b.count - count };
                return static_cast<i32>(off);
            }
        }
        // Bump allocate.
        if (m_nextFree + count <= m_capacity) {
            u32 off = m_nextFree;
            m_nextFree += count;
            return static_cast<i32>(off);
        }
        return -1;
    }

    /// Free a block with coalescing.
    void free(u32 offset, u32 count) {
        if (count == 0) return;
        u32 mOff = offset, mCnt = count;
        for (usize i = 0; i < m_freeBlocks.size(); ) {
            if (m_freeBlocks[i].offset + m_freeBlocks[i].count == mOff) {
                mOff = m_freeBlocks[i].offset; mCnt += m_freeBlocks[i].count;
                m_freeBlocks.erase(m_freeBlocks.begin() + i);
            } else if (mOff + mCnt == m_freeBlocks[i].offset) {
                mCnt += m_freeBlocks[i].count;
                m_freeBlocks.erase(m_freeBlocks.begin() + i);
            } else ++i;
        }
        if (mOff + mCnt == m_nextFree)
            m_nextFree = mOff;
        else
            m_freeBlocks.push_back({ mOff, mCnt });
    }

    D3D12_CPU_DESCRIPTOR_HANDLE getCpuHandle(u32 offset) const {
        D3D12_CPU_DESCRIPTOR_HANDLE h{}; h.ptr = m_cpuStart.ptr + static_cast<SIZE_T>(offset) * m_incrementSize; return h;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE getGpuHandle(u32 offset) const {
        D3D12_GPU_DESCRIPTOR_HANDLE h{}; h.ptr = m_gpuStart.ptr + static_cast<UINT64>(offset) * m_incrementSize; return h;
    }

    void destroy() { m_heap.Reset(); m_freeBlocks.clear(); }

    [[nodiscard]] ID3D12DescriptorHeap* heap()          const { return m_heap.Get(); }
    [[nodiscard]] u32                   incrementSize()  const { return m_incrementSize; }

private:
    struct FreeBlock { u32 offset; u32 count; };

    ComPtr<ID3D12DescriptorHeap>  m_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE   m_cpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE   m_gpuStart{};
    u32                           m_incrementSize = 0;
    u32                           m_capacity  = 0;
    u32                           m_nextFree  = 0;
    std::vector<FreeBlock>              m_freeBlocks;
};

} // namespace draco::rhi::dx12
