/// Vulkan implementation of Surface.
/// Wraps VkSurfaceKHR + parent VkInstance for cleanup.

module;

#include "VkIncludes.h"

export module rhi.vk:surface;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::vk {

class VkSurfaceImpl : public Surface {
public:
    VkSurfaceImpl(VkSurfaceKHR surface, VkInstance instance)
        : m_surface(surface), m_instance(instance) {}

    [[nodiscard]] VkSurfaceKHR handle() const { return m_surface; }

    void destroy() {
        if (m_surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }
    }

private:
    VkSurfaceKHR m_surface  = VK_NULL_HANDLE;
    VkInstance   m_instance = VK_NULL_HANDLE;
};

} // namespace draco::rhi::vk
