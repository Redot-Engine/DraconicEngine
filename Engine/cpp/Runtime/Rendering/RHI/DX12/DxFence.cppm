/// DX12 implementation of Fence using ID3D12Fence.

module;

#include "DxIncludes.h"

export module rhi.dx12:fence;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxFenceImpl : public Fence {
public:
    Status init(ID3D12Device* device, u64 initialValue) {
        HRESULT hr = device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        if (FAILED(hr)) {
            logErrorf("DxFence: CreateFence failed (0x%08X)", static_cast<unsigned>(hr));
            return ErrorCode::Unknown;
        }
        m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        return ErrorCode::Ok;
    }

    u64 completedValue() override {
        return m_fence->GetCompletedValue();
    }

    bool wait(u64 value, u64 timeoutNs) override {
        if (m_fence->GetCompletedValue() >= value) return true;
        m_fence->SetEventOnCompletion(value, m_event);
        DWORD timeoutMs = (timeoutNs == ~0ull) ? INFINITE : static_cast<DWORD>(timeoutNs / 1000000);
        return WaitForSingleObject(m_event, timeoutMs) == WAIT_OBJECT_0;
    }

    void cleanup() {
        if (m_event) { CloseHandle(m_event); m_event = nullptr; }
        m_fence.Reset();
    }

    // ---- Internal ----
    [[nodiscard]] ID3D12Fence* handle() const { return m_fence.Get(); }

    void signal(ID3D12CommandQueue* queue, u64 value) {
        queue->Signal(m_fence.Get(), value);
    }

private:
    ComPtr<ID3D12Fence> m_fence;
    HANDLE              m_event = nullptr;
};

} // namespace draco::rhi::dx12
