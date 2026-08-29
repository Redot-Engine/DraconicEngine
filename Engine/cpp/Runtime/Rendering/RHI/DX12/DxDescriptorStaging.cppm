/// Bump-allocating staging region within a GPU-visible descriptor heap.
/// Copies bind group descriptors from CPU heap into GPU heap at bind time.

module;

#include "DxIncludes.h"
#include <vector>

#include <algorithm>

export module rhi.dx12:descriptor_staging;

import core.stdtypes;
import core.status;
import :gpu_descriptor_heap;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxDescriptorStaging {
public:
    DxDescriptorStaging() = default;

    void init(DxGpuDescriptorHeap* cpuHeap, DxGpuDescriptorHeap* gpuHeap,
              ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, u32 initialCapacity) {
        m_cpuHeap  = cpuHeap;
        m_gpuHeap  = gpuHeap;
        m_device   = device;
        m_heapType = heapType;
        m_capacity = initialCapacity;
    }

    /// Copies `count` descriptors from `srcOffset` in CPU heap into GPU staging.
    /// Returns the staging offset in the GPU heap, or -1 on failure.
    i32 copyFrom(u32 srcOffset, u32 count) {
        if (count == 0) return -1;

        // Lazy allocation.
        if (m_blockOffset < 0) {
            m_blockOffset = m_gpuHeap->allocate(m_capacity);
            if (m_blockOffset < 0) return -1;
            m_current = 0;
        }

        // Grow if needed: retire current block, allocate bigger.
        if (m_current + count > m_capacity) {
            u32 newCap = std::max(m_capacity * 2, m_current + count);
            i32 newBlock = m_gpuHeap->allocate(newCap);
            if (newBlock < 0) return -1;
            m_retiredBlocks.push_back({ m_blockOffset, m_capacity });
            m_blockOffset = newBlock;
            m_capacity = newCap;
            m_current = 0;
        }

        u32 dstOffset = static_cast<u32>(m_blockOffset) + m_current;
        m_device->CopyDescriptorsSimple(count,
            m_gpuHeap->getCpuHandle(dstOffset),
            m_cpuHeap->getCpuHandle(srcOffset),
            m_heapType);
        m_current += count;
        return static_cast<i32>(dstOffset);
    }

    /// Resets bump pointer. Called when command pool resets after fence wait.
    void reset() {
        m_current = 0;
        for (auto& b : m_retiredBlocks)
            m_gpuHeap->free(static_cast<u32>(b.offset), b.capacity);
        m_retiredBlocks.clear();
    }

    /// Frees all blocks.
    void destroy() {
        if (m_blockOffset >= 0) {
            m_gpuHeap->free(static_cast<u32>(m_blockOffset), m_capacity);
            m_blockOffset = -1;
        }
        for (auto& b : m_retiredBlocks)
            m_gpuHeap->free(static_cast<u32>(b.offset), b.capacity);
        m_retiredBlocks.clear();
    }

private:
    struct RetiredBlock { i32 offset; u32 capacity; };

    DxGpuDescriptorHeap*         m_cpuHeap  = nullptr;
    DxGpuDescriptorHeap*         m_gpuHeap  = nullptr;
    ID3D12Device*                m_device   = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE   m_heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    i32                          m_blockOffset = -1;
    u32                          m_capacity = 0;
    u32                          m_current  = 0;
    std::vector<RetiredBlock>          m_retiredBlocks;
};

} // namespace draco::rhi::dx12
