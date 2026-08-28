/// Vulkan implementation of Fence (timeline semaphore).

module;

#include "VkIncludes.h"

export module rhi.vk:fence;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::vk {

class VkFenceImpl : public Fence {
public:
    Status init(VkDevice device, u64 initialValue) {
        m_device = device;

        VkSemaphoreTypeCreateInfo typeInfo{};
        typeInfo.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeInfo.initialValue  = initialValue;

        VkSemaphoreCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        ci.pNext = &typeInfo;

        if (vkCreateSemaphore(device, &ci, nullptr, &m_semaphore) != VK_SUCCESS) return ErrorCode::Unknown;
        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device) {
        if (m_semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, m_semaphore, nullptr);
            m_semaphore = VK_NULL_HANDLE;
        }
    }

    // ---- Fence interface ----
    u64 completedValue() override {
        u64 value = 0;
        vkGetSemaphoreCounterValue(m_device, m_semaphore, &value);
        return value;
    }

    bool wait(u64 value, u64 timeoutNs) override {
        VkSemaphoreWaitInfo wi{};
        wi.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        wi.semaphoreCount = 1;
        wi.pSemaphores    = &m_semaphore;
        wi.pValues        = &value;
        return vkWaitSemaphores(m_device, &wi, timeoutNs) == VK_SUCCESS;
    }

    [[nodiscard]] VkSemaphore handle() const { return m_semaphore; }

private:
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
    VkDevice    m_device    = VK_NULL_HANDLE;
};

} // namespace draco::rhi::vk
