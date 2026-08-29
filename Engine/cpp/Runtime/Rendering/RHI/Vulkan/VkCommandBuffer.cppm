/// Vulkan implementation of CommandBuffer.

module;

#include "VkIncludes.h"

export module rhi.vk:command_buffer;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::vk {

class VkCommandBufferImpl : public CommandBuffer {
public:
    explicit VkCommandBufferImpl(VkCommandBuffer cmdBuf) : m_cmdBuf(cmdBuf) {}

    [[nodiscard]] VkCommandBuffer handle() const { return m_cmdBuf; }

private:
    VkCommandBuffer m_cmdBuf = VK_NULL_HANDLE;
};

} // namespace draco::rhi::vk
