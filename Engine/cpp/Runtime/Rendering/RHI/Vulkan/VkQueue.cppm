/// Vulkan implementation of Queue.

module;

#include "VkIncludes.h"
#include <vector>
#include <span>


export module rhi.vk:queue;

import core.stdtypes;
import core.status;
import rhi;
import :command_buffer;
import :fence;
import :transfer_batch;

using namespace draco;

export namespace draco::rhi::vk {

class VkDeviceImpl; // forward

class VkQueueImpl : public Queue {
public:
    VkQueueImpl(VkQueue queue, QueueType type, u32 familyIndex, f32 tsPeriod, VkDeviceImpl* device, VkDevice vkDevice, VkPhysicalDevice physDevice)
        : m_queue(queue), m_familyIndex(familyIndex), m_tsPeriod(tsPeriod), m_device(device), m_vkDevice(vkDevice), m_physDevice(physDevice)
    { queueType = type; }

    // ---- Queue interface ----

    void submit(std::span<CommandBuffer* const> cmdBufs) override {
        if (cmdBufs.size() == 0) return;
        std::vector<VkCommandBuffer> bufs(cmdBufs.size());
        for (usize i = 0; i < cmdBufs.size(); ++i)
            bufs[i] = static_cast<VkCommandBufferImpl*>(cmdBufs[i])->handle();

        VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = static_cast<u32>(bufs.size());
        si.pCommandBuffers    = bufs.data();
        vkQueueSubmit(m_queue, 1, &si, VK_NULL_HANDLE);
    }

    void submit(std::span<CommandBuffer* const> cmdBufs, Fence* signalFence, u64 signalValue) override;

    void submit(std::span<CommandBuffer* const> cmdBufs,
                std::span<Fence* const> waitFences, std::span<const u64> waitValues,
                Fence* signalFence, u64 signalValue) override;


    void waitIdle() override { vkQueueWaitIdle(m_queue); }

    Status createTransferBatch(TransferBatch*& out) override {
        out = new VkTransferBatchImpl(m_vkDevice, m_queue, m_familyIndex, m_physDevice);
        return ErrorCode::Ok;
    }
    void destroyTransferBatch(TransferBatch*& batch) override {
        if (batch) { static_cast<VkTransferBatchImpl*>(batch)->destroy(); delete batch; batch = nullptr; }
    }

    f32 timestampPeriod() const override { return m_tsPeriod; }

    // ---- Internal ----
    [[nodiscard]] VkQueue handle()      const { return m_queue; }
    [[nodiscard]] u32     familyIndex() const { return m_familyIndex; }

private:
    VkQueue       m_queue       = VK_NULL_HANDLE;
    u32           m_familyIndex = 0;
    f32           m_tsPeriod    = 0.0f;
    VkDeviceImpl*    m_device      = nullptr;
    VkDevice         m_vkDevice    = VK_NULL_HANDLE;
    VkPhysicalDevice m_physDevice  = VK_NULL_HANDLE;
};

} // namespace draco::rhi::vk
