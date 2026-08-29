/// Vulkan implementation of SwapChain.

module;

#include "VkIncludes.h"
#include <vector>
#include <string>
#include <algorithm>


export module rhi.vk:swap_chain;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :adapter;
import :surface;
import :texture;
import :texture_view;

using namespace draco;

export namespace draco::rhi::vk {

// Minimal reverse format mapping for swap chain format negotiation.
inline TextureFormat fromVkFormat(VkFormat f) {
    switch (f) {
    case VK_FORMAT_R8G8B8A8_UNORM: return TextureFormat::RGBA8Unorm;
    case VK_FORMAT_R8G8B8A8_SRGB:  return TextureFormat::RGBA8UnormSrgb;
    case VK_FORMAT_B8G8R8A8_UNORM: return TextureFormat::BGRA8Unorm;
    case VK_FORMAT_B8G8R8A8_SRGB:  return TextureFormat::BGRA8UnormSrgb;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return TextureFormat::RGBA16Float;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return TextureFormat::RGB10A2Unorm;
    default: return TextureFormat::Undefined;
    }
}

class VkDeviceImpl; // forward

class VkSwapChainImpl : public SwapChain {
public:
    Status init(VkDevice device, VkPhysicalDevice physDevice, VkSurfaceKHR surface,
                const SwapChainDesc& desc, VkDeviceImpl* owner) {
        m_device     = device;
        m_physDevice = physDevice;
        m_surface    = surface;
        m_owner      = owner;
        m_presentMode= desc.presentMode;
        return createSwapChain(desc.width, desc.height, desc.format, desc.bufferCount, VK_NULL_HANDLE);
    }

    // ---- SwapChain interface ----
    TextureFormat format()            const override { return m_format; }
    u32           width()             const override { return m_width; }
    u32           height()            const override { return m_height; }
    u32           bufferCount()       const override { return m_bufferCount; }
    u32           currentImageIndex() const override { return m_currentImageIndex; }

    Status acquireNextImage() override;
    Texture*     currentTexture()     override { return m_currentImageIndex < m_textures.size() ? m_textures[m_currentImageIndex] : nullptr; }
    TextureView* currentTextureView() override { return m_currentImageIndex < m_views.size()    ? m_views[m_currentImageIndex]    : nullptr; }
    Status present(Queue* queue) override;
    Status resize(u32 w, u32 h) override;

    void cleanup();

    // ---- Internal ----
    [[nodiscard]] VkSwapchainKHR handle() const { return m_swapchain; }

    // Semaphore accessors for queue submit integration.
    VkSemaphore currentAcquireSemaphore() const { return m_acquireSems[m_frameIndex]; }
    VkSemaphore currentPresentSemaphore() const { return m_presentSems[m_currentImageIndex]; }

private:
    Status createSwapChain(u32 w, u32 h, TextureFormat reqFormat, u32 reqCount, VkSwapchainKHR old);
    Status retrieveImages(VkFormat format);
    void   createSyncObjects();
    void   cleanupImages();
    void   destroySyncObjects();

    VkSurfaceFormatKHR chooseSurfaceFormat(TextureFormat requested);
    VkPresentModeKHR   choosePresentMode(PresentMode requested);

    VkDevice         m_device     = VK_NULL_HANDLE;
    VkPhysicalDevice m_physDevice = VK_NULL_HANDLE;
    VkSurfaceKHR     m_surface    = VK_NULL_HANDLE;
    VkSwapchainKHR   m_swapchain  = VK_NULL_HANDLE;
    VkDeviceImpl*    m_owner      = nullptr;

    TextureFormat m_format      = TextureFormat::Undefined;
    PresentMode   m_presentMode = PresentMode::Fifo;
    u32 m_width  = 0, m_height = 0, m_bufferCount = 0;
    u32 m_currentImageIndex = 0;
    u32 m_frameIndex        = 0;

    std::vector<VkTextureImpl*>     m_textures;
    std::vector<VkTextureViewImpl*> m_views;
    std::vector<VkSemaphore>        m_acquireSems;
    std::vector<VkSemaphore>        m_presentSems;
};

// ---- Implementation (inline in module) ----

inline Status VkSwapChainImpl::createSwapChain(u32 w, u32 h, TextureFormat reqFormat, u32 reqCount, VkSwapchainKHR old) {
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physDevice, m_surface, &caps);

    if (caps.currentExtent.width != ~0u) { m_width = caps.currentExtent.width; m_height = caps.currentExtent.height; }
    else { m_width = std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width);
           m_height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height); }
    if (m_width == 0 || m_height == 0) return ErrorCode::Unknown;

    auto presentMode = choosePresentMode(m_presentMode);

    // Mailbox needs a 3rd image to race ahead of the display without stalling; 2 is fine for
    // FIFO/Immediate.
    u32 wantCount = reqCount;
    if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR && wantCount < 3) { wantCount = 3; }
    m_bufferCount = std::max(wantCount, caps.minImageCount);
    if (caps.maxImageCount > 0) m_bufferCount = std::min(m_bufferCount, caps.maxImageCount);

    auto surfFmt = chooseSurfaceFormat(reqFormat);
    m_format = fromVkFormat(surfFmt.format);
    if (m_format == TextureFormat::Undefined) m_format = reqFormat;

    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkCompositeAlphaFlagBitsKHR compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = m_surface;
    ci.minImageCount    = m_bufferCount;
    ci.imageFormat      = surfFmt.format;
    ci.imageColorSpace  = surfFmt.colorSpace;
    ci.imageExtent      = { m_width, m_height };
    ci.imageArrayLayers = 1;
    ci.imageUsage       = usage;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = compositeAlpha;
    ci.presentMode      = presentMode;
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = old;

    if (vkCreateSwapchainKHR(m_device, &ci, nullptr, &m_swapchain) != VK_SUCCESS) return ErrorCode::Unknown;
    if (old != VK_NULL_HANDLE) vkDestroySwapchainKHR(m_device, old, nullptr);

    if (retrieveImages(surfFmt.format) != ErrorCode::Ok) return ErrorCode::Unknown;
    createSyncObjects();
    m_frameIndex = 0;
    return ErrorCode::Ok;
}

inline VkSurfaceFormatKHR VkSwapChainImpl::chooseSurfaceFormat(TextureFormat requested) {
    u32 count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physDevice, m_surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> fmts(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physDevice, m_surface, &count, fmts.data());

    VkFormat desired = toVkFormat(requested);
    for (auto& f : fmts) if (f.format == desired) return f;
    for (auto& f : fmts) if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
    for (auto& f : fmts) if (f.format == VK_FORMAT_B8G8R8A8_UNORM) return f;
    return fmts[0];
}

inline VkPresentModeKHR VkSwapChainImpl::choosePresentMode(PresentMode requested) {
    u32 count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physDevice, m_surface, &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physDevice, m_surface, &count, modes.data());
    auto has = [&](VkPresentModeKHR m) { for (auto x : modes) if (x == m) return true; return false; };

    const VkPresentModeKHR desired = toVkPresentMode(requested);
    if (has(desired)) { return desired; }

    // Requested mode unavailable: preserve the request's INTENT rather than dropping straight to
    // vsync. Immediate and Mailbox both mean "don't block on the refresh" - so fall back to whichever
    // uncapped mode the surface does expose (Wayland commonly offers Mailbox but NOT Immediate) before
    // settling for FIFO. FIFO is the only mode guaranteed present by the spec.
    VkPresentModeKHR chosen = VK_PRESENT_MODE_FIFO_KHR;
    if (requested == PresentMode::Immediate || requested == PresentMode::Mailbox) {
        if (has(VK_PRESENT_MODE_MAILBOX_KHR))        { chosen = VK_PRESENT_MODE_MAILBOX_KHR; }
        else if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) { chosen = VK_PRESENT_MODE_IMMEDIATE_KHR; }
    } else if (requested == PresentMode::FifoRelaxed && has(VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
        chosen = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    }

    logWarningf("[swapchain] requested present mode (vk %u) unavailable; using vk %u",
                static_cast<u32>(desired), static_cast<u32>(chosen));
    return chosen;
}

inline Status VkSwapChainImpl::retrieveImages(VkFormat format) {
    u32 imgCount = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imgCount, nullptr);
    std::vector<VkImage> images(imgCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &imgCount, images.data());
    m_bufferCount = imgCount;

    TextureFormat texFmt = fromVkFormat(format);
    if (texFmt == TextureFormat::Undefined) texFmt = m_format;

    for (u32 i = 0; i < imgCount; ++i) {
        TextureDesc td{}; td.dimension = TextureDimension::Texture2D; td.format = texFmt;
        td.width = m_width; td.height = m_height; td.arrayLayerCount = 1; td.mipLevelCount = 1;
        td.sampleCount = 1; td.usage = TextureUsage::RenderTarget;

        auto* tex = new VkTextureImpl();
        tex->initFromExisting(images[i], td);
        m_textures.push_back(tex);

        TextureViewDesc vd{}; vd.format = texFmt; vd.dimension = TextureViewDimension::Texture2D;
        vd.mipLevelCount = 1; vd.arrayLayerCount = 1;
        auto* view = new VkTextureViewImpl();
        if (view->init(m_device, tex, vd) != ErrorCode::Ok) { delete view; return ErrorCode::Unknown; }
        m_views.push_back(view);
    }
    return ErrorCode::Ok;
}

inline void VkSwapChainImpl::createSyncObjects() {
    VkSemaphoreCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (u32 i = 0; i < m_bufferCount; ++i) {
        VkSemaphore a = VK_NULL_HANDLE, p = VK_NULL_HANDLE;
        vkCreateSemaphore(m_device, &ci, nullptr, &a);
        vkCreateSemaphore(m_device, &ci, nullptr, &p);
        m_acquireSems.push_back(a);
        m_presentSems.push_back(p);
    }
}

inline void VkSwapChainImpl::cleanupImages() {
    for (auto* v : m_views)    { v->cleanup(m_device); delete v; } m_views.clear();
    for (auto* t : m_textures) { t->cleanup(m_device); delete t; } m_textures.clear();
}

inline void VkSwapChainImpl::destroySyncObjects() {
    for (auto s : m_acquireSems) { vkDestroySemaphore(m_device, s, nullptr); }
    m_acquireSems.clear();
    for (auto s : m_presentSems) { vkDestroySemaphore(m_device, s, nullptr); }
    m_presentSems.clear();
}

inline Status VkSwapChainImpl::resize(u32 w, u32 h) {
    vkDeviceWaitIdle(m_device);
    cleanupImages();
    destroySyncObjects();
    return createSwapChain(w, h, m_format, m_bufferCount, m_swapchain);
}

inline void VkSwapChainImpl::cleanup() {
    vkDeviceWaitIdle(m_device);
    cleanupImages();
    destroySyncObjects();
    if (m_swapchain != VK_NULL_HANDLE) { vkDestroySwapchainKHR(m_device, m_swapchain, nullptr); m_swapchain = VK_NULL_HANDLE; }
}

} // namespace draco::rhi::vk
