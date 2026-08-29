/// Vulkan implementation of CommandPool.

module;

#include "VkIncludes.h"
#include <vector>


export module rhi.vk:command_pool;

import core.stdtypes;
import core.status;
import rhi;
import :adapter;
import :command_buffer;

using namespace draco;

export namespace draco::rhi::vk {

class VkDeviceImpl;        // forward
class VkCommandEncoderImpl; // forward

class VkCommandPoolImpl : public CommandPool {
public:
    Status init(VkDevice device, VkAdapterImpl* adapter, QueueType queueType) {
        m_device = device;

        i32 familyIndex = adapter->findQueueFamily(queueType);
        if (familyIndex < 0) return ErrorCode::Unknown;
        m_familyIndex = static_cast<u32>(familyIndex);

        VkCommandPoolCreateInfo ci{};
        ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        ci.queueFamilyIndex = m_familyIndex;

        if (vkCreateCommandPool(device, &ci, nullptr, &m_pool) != VK_SUCCESS) return ErrorCode::Unknown;
        return ErrorCode::Ok;
    }

    // ---- CommandPool interface ----
    Status createEncoder(CommandEncoder*& out) override;
    void   destroyEncoder(CommandEncoder*& encoder) override;
    void   reset() override;

    void cleanup() {
        for (auto* cb : m_trackedBuffers) delete cb;
        m_trackedBuffers.clear();
        m_freeHandles.clear();
        for (auto* e : m_trackedBundleEncoders) delete e;   // each frees its produced bundle
        m_trackedBundleEncoders.clear();
        m_liveSecondaries.clear();
        m_freeSecondaries.clear();

        if (m_pool != VK_NULL_HANDLE) { vkDestroyCommandPool(m_device, m_pool, nullptr); m_pool = VK_NULL_HANDLE; }
    }

    // Called by encoder's finish() to register the command buffer.
    void trackCommandBuffer(VkCommandBufferImpl* cb) { m_trackedBuffers.push_back(cb); }

    // ---- render bundles (secondary command buffers) ----

    // Allocate (or recycle) a SECONDARY command buffer for a render bundle encoder.
    [[nodiscard]] VkCommandBuffer acquireSecondary() {
        VkCommandBuffer cb = VK_NULL_HANDLE;
        if (!m_freeSecondaries.empty()) { cb = m_freeSecondaries.back(); m_freeSecondaries.pop_back(); }
        else {
            VkCommandBufferAllocateInfo ai{};
            ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            ai.commandPool        = m_pool;
            ai.level              = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
            ai.commandBufferCount = 1;
            if (vkAllocateCommandBuffers(m_device, &ai, &cb) != VK_SUCCESS) return VK_NULL_HANDLE;
        }
        m_liveSecondaries.push_back(cb);   // recycled on Reset
        return cb;
    }

    // Track a bundle-encoder wrapper so it (and the bundle it owns) is freed on Reset.
    void trackBundleEncoder(RenderBundleEncoder* e) { m_trackedBundleEncoders.push_back(e); }

    [[nodiscard]] VkCommandPool handle() const { return m_pool; }
    [[nodiscard]] VkDevice      vkDevice() const { return m_device; }

    // Stored so the encoder can access it.
    VkDeviceImpl* ownerDevice = nullptr;

private:
    VkDevice                          m_device      = VK_NULL_HANDLE;
    VkCommandPool                     m_pool        = VK_NULL_HANDLE;
    u32                               m_familyIndex = 0;
    std::vector<VkCommandBuffer>      m_freeHandles;
    std::vector<VkCommandBufferImpl*> m_trackedBuffers;
    std::vector<VkCommandBuffer>      m_freeSecondaries;        // recyclable secondary handles
    std::vector<VkCommandBuffer>      m_liveSecondaries;        // handed out this cycle
    std::vector<RenderBundleEncoder*> m_trackedBundleEncoders;  // wrappers freed on reset
};

} // namespace draco::rhi::vk
