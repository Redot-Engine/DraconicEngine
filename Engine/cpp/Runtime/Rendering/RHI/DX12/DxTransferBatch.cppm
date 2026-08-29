/// DX12 implementation of TransferBatch.

module;

#include "DxIncludes.h"
#include <span>

#include <cstring>
#include <vector>

export module rhi.dx12:transfer_batch;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :buffer;
import :texture;
import :fence;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxQueueImpl; // forward

class DxTransferBatchImpl : public TransferBatch {
public:
    Status init(ID3D12Device* device, ID3D12CommandQueue* queue, QueueType queueType) {
        m_device = device;
        m_queue  = queue;

        HRESULT hr = device->CreateCommandAllocator(
            toCommandListType(queueType),
            IID_PPV_ARGS(&m_allocator));
        if (FAILED(hr)) {
            logErrorf("DxTransferBatch: CreateCommandAllocator failed (0x%08X)", static_cast<unsigned>(hr));
            return ErrorCode::Unknown;
        }

        hr = device->CreateCommandList(0,
            toCommandListType(queueType),
            m_allocator.Get(), nullptr,
            IID_PPV_ARGS(&m_cmdList));
        if (FAILED(hr)) {
            logErrorf("DxTransferBatch: CreateCommandList failed (0x%08X)", static_cast<unsigned>(hr));
            return ErrorCode::Unknown;
        }

        // Command list starts open; close it until we need it.
        m_cmdList->Close();

        // Create fence for synchronous submit.
        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        if (FAILED(hr)) {
            logErrorf("DxTransferBatch: CreateFence failed (0x%08X)", static_cast<unsigned>(hr));
            return ErrorCode::Unknown;
        }

        m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        m_fenceValue = 0;

        return ErrorCode::Ok;
    }

    // ---- TransferBatch interface ----

    void writeBuffer(Buffer* dst, u64 dstOffset, std::span<const u8> data) override {
        auto* dxDst = static_cast<DxBufferImpl*>(dst);
        if (!dxDst || data.size() == 0) return;

        ensureRecording();

        // Create upload-heap staging buffer.
        u64 stagingSize = static_cast<u64>(data.size());
        ComPtr<ID3D12Resource> staging;

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC rd{};
        rd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width            = stagingSize;
        rd.Height           = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels        = 1;
        rd.Format           = DXGI_FORMAT_UNKNOWN;
        rd.SampleDesc       = { 1, 0 };
        rd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        rd.Flags            = D3D12_RESOURCE_FLAG_NONE;

        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE,
            &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&staging));
        if (FAILED(hr)) return;

        // Map and copy data.
        void* mapped = nullptr;
        staging->Map(0, nullptr, &mapped);
        std::memcpy(mapped, data.data(), data.size());
        staging->Unmap(0, nullptr);

        m_stagingBuffers.push_back(std::move(staging));

        // Record copy command.
        m_cmdList->CopyBufferRegion(dxDst->handle(), dstOffset,
            m_stagingBuffers.back().Get(), 0, stagingSize);
    }

    void writeTexture(Texture* dst, std::span<const u8> data,
                      const TextureDataLayout& layout, Extent3D extent,
                      u32 mipLevel, u32 arrayLayer) override {
        auto* dxTex = static_cast<DxTextureImpl*>(dst);
        if (!dxTex || data.size() == 0) return;

        ensureRecording();

        // Calculate aligned row pitch (D3D12 requires 256-byte row alignment).
        u32 alignedRowPitch = (layout.bytesPerRow + 255) & ~u32(255);
        u32 rowsPerImage    = (layout.rowsPerImage > 0) ? layout.rowsPerImage : extent.height;
        u64 stagingSize     = static_cast<u64>(alignedRowPitch) * rowsPerImage * extent.depth;

        ComPtr<ID3D12Resource> staging;

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC rd{};
        rd.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width            = stagingSize;
        rd.Height           = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels        = 1;
        rd.Format           = DXGI_FORMAT_UNKNOWN;
        rd.SampleDesc       = { 1, 0 };
        rd.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        rd.Flags            = D3D12_RESOURCE_FLAG_NONE;

        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE,
            &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&staging));
        if (FAILED(hr)) return;

        // Map and copy data row by row (handles pitch alignment).
        void* mapped = nullptr;
        staging->Map(0, nullptr, &mapped);

        const u8* srcPtr = data.data() + layout.offset;
        u8*       dstPtr = static_cast<u8*>(mapped);
        for (u32 z = 0; z < extent.depth; ++z) {
            for (u32 row = 0; row < rowsPerImage; ++row) {
                std::memcpy(
                    dstPtr + (z * rowsPerImage + row) * alignedRowPitch,
                    srcPtr + (z * rowsPerImage + row) * layout.bytesPerRow,
                    layout.bytesPerRow);
            }
        }

        staging->Unmap(0, nullptr);
        m_stagingBuffers.push_back(std::move(staging));

        // Transition texture to copy dest.
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource   = dxTex->handle();
        barrier.Transition.StateBefore = dxTex->currentState();
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        if (dxTex->currentState() != D3D12_RESOURCE_STATE_COPY_DEST)
            m_cmdList->ResourceBarrier(1, &barrier);

        u32 subresource = mipLevel + arrayLayer * dxTex->desc.mipLevelCount;

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource                          = m_stagingBuffers.back().Get();
        srcLoc.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint.Offset             = 0;
        srcLoc.PlacedFootprint.Footprint.Format   = toDxgiFormat(dxTex->desc.format);
        srcLoc.PlacedFootprint.Footprint.Width    = extent.width;
        srcLoc.PlacedFootprint.Footprint.Height   = extent.height;
        srcLoc.PlacedFootprint.Footprint.Depth    = extent.depth;
        srcLoc.PlacedFootprint.Footprint.RowPitch = alignedRowPitch;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource        = dxTex->handle();
        dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = subresource;

        m_cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        // Transition back to common.
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COMMON;
        m_cmdList->ResourceBarrier(1, &barrier);
        dxTex->setState(D3D12_RESOURCE_STATE_COMMON);
    }

    Status submit() override {
        if (!m_isRecording) return ErrorCode::Ok;

        m_cmdList->Close();
        m_isRecording = false;

        ID3D12CommandList* lists[] = { m_cmdList.Get() };
        m_queue->ExecuteCommandLists(1, lists);

        // Wait for completion.
        ++m_fenceValue;
        m_queue->Signal(m_fence.Get(), m_fenceValue);
        if (m_fence->GetCompletedValue() < m_fenceValue) {
            m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }

        releaseStagingBuffers();
        return ErrorCode::Ok;
    }

    Status submitAsync(Fence* fence, u64 signalValue) override {
        if (!m_isRecording) return ErrorCode::Ok;

        m_cmdList->Close();
        m_isRecording = false;

        ID3D12CommandList* lists[] = { m_cmdList.Get() };
        m_queue->ExecuteCommandLists(1, lists);

        if (auto* dxFence = static_cast<DxFenceImpl*>(fence))
            m_queue->Signal(dxFence->handle(), signalValue);

        // Note: staging buffers can't be released until GPU is done.
        // Caller must wait on the fence before calling reset().
        return ErrorCode::Ok;
    }

    void reset() override {
        releaseStagingBuffers();
    }

    void destroy() override {
        releaseStagingBuffers();

        if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
        m_fence.Reset();
        m_cmdList.Reset();
        m_allocator.Reset();
    }

private:
    void ensureRecording() {
        if (!m_isRecording) {
            m_allocator->Reset();
            m_cmdList->Reset(m_allocator.Get(), nullptr);
            m_isRecording = true;
        }
    }

    void releaseStagingBuffers() {
        m_stagingBuffers.clear();
    }

    ID3D12Device*                       m_device  = nullptr;
    ID3D12CommandQueue*                 m_queue   = nullptr;
    ComPtr<ID3D12CommandAllocator>      m_allocator;
    ComPtr<ID3D12GraphicsCommandList>   m_cmdList;
    bool                                m_isRecording = false;

    // ComPtr's operator overloads are incompatible with Array's placement-new.
    std::vector<ComPtr<ID3D12Resource>>  m_stagingBuffers;

    ComPtr<ID3D12Fence>                 m_fence;
    u64                                 m_fenceValue = 0;
    HANDLE                              m_fenceEvent = nullptr;
};

} // namespace draco::rhi::dx12
