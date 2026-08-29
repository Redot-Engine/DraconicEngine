/// DX12 implementation of CommandBuffer.
/// Simple wrapper for a closed ID3D12GraphicsCommandList.

module;

#include "DxIncludes.h"

export module rhi.dx12:command_buffer;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::dx12 {

class DxCommandBufferImpl : public CommandBuffer {
public:
    explicit DxCommandBufferImpl(ID3D12GraphicsCommandList* cmdList) : m_cmdList(cmdList) {}

    [[nodiscard]] ID3D12GraphicsCommandList* handle() const { return m_cmdList; }

    void release() {
        if (m_cmdList) { m_cmdList->Release(); m_cmdList = nullptr; }
    }

private:
    ID3D12GraphicsCommandList* m_cmdList = nullptr; // raw, released via release()
};

} // namespace draco::rhi::dx12
