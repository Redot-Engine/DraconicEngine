/// Vulkan implementation of Texture.

module;

#include "VkIncludes.h"
#include <vector>
#include <algorithm>


export module rhi.vk:texture;

import core.stdtypes;
import core.status;
import rhi;
import :adapter;
import :conversions;

using namespace draco;

export namespace draco::rhi::vk {

class VkTextureImpl : public Texture {
public:
    /// Initialize from a TextureDesc (creates VkImage + allocates memory).
    Status init(VkDevice device, VkAdapterImpl* adapter, const TextureDesc& d) {
        desc = d;

        VkImageCreateInfo ci{};
        ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType     = toVkImageType(d.dimension);
        ci.format        = toVkFormat(d.format);
        ci.extent        = { d.width, d.height, d.depth };
        ci.mipLevels     = d.mipLevelCount;
        ci.arrayLayers   = d.arrayLayerCount;
        ci.samples       = toVkSampleCount(d.sampleCount);
        ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
        ci.usage         = toVkImageUsage(d.usage);
        ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (d.arrayLayerCount >= 6 && d.dimension == TextureDimension::Texture2D)
            ci.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        if (vkCreateImage(device, &ci, nullptr, &m_image) != VK_SUCCESS) return ErrorCode::Unknown;

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device, m_image, &memReqs);

        i32 memType = adapter->findMemoryType(
            static_cast<u32>(memReqs.memoryTypeBits), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType < 0) {
            vkDestroyImage(device, m_image, nullptr); m_image = VK_NULL_HANDLE;
            return ErrorCode::Unknown;
        }

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = memReqs.size;
        ai.memoryTypeIndex = static_cast<u32>(memType);

        if (vkAllocateMemory(device, &ai, nullptr, &m_memory) != VK_SUCCESS) {
            vkDestroyImage(device, m_image, nullptr); m_image = VK_NULL_HANDLE;
            return ErrorCode::Unknown;
        }

        vkBindImageMemory(device, m_image, m_memory, 0);
        return ErrorCode::Ok;
    }

    /// Initialize from an existing VkImage (e.g. swap chain). Does not own the image.
    void initFromExisting(VkImage image, const TextureDesc& d) {
        m_image     = image;
        desc       = d;
        m_ownsImage = false;
    }

    void cleanup(VkDevice device) {
        if (m_memory != VK_NULL_HANDLE) { vkFreeMemory(device, m_memory, nullptr); m_memory = VK_NULL_HANDLE; }
        if (m_ownsImage && m_image != VK_NULL_HANDLE) vkDestroyImage(device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        m_subresourceLayouts.clear();
    }

    // ---- Internal ----
    [[nodiscard]] VkImage  handle()   const { return m_image; }
    [[nodiscard]] VkFormat vkFormat() const { return toVkFormat(desc.format); }

    /// Whole-resource layout (uniform fast path).
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    /// Get layout for a specific subresource.
    VkImageLayout getSubresourceLayout(u32 mip, u32 layer) const {
        if (m_subresourceLayouts.empty()) return currentLayout;
        u32 idx = mip + layer * desc.mipLevelCount;
        if (idx >= static_cast<u32>(m_subresourceLayouts.size())) return currentLayout;
        return m_subresourceLayouts[idx];
    }

    /// Update layout for a subresource range. Promotes to per-subresource
    /// tracking when needed, collapses back to uniform when all match.
    void setSubresourceLayout(u32 baseMip, u32 mipCount, u32 baseLayer, u32 layerCount,
                              VkImageLayout layout) {
        u32 totalMips   = desc.mipLevelCount;
        u32 totalLayers = std::max(desc.arrayLayerCount, 1u);
        u32 mipEnd   = (mipCount   == ~0u) ? totalMips   : std::min(baseMip   + mipCount,   totalMips);
        u32 layerEnd = (layerCount == ~0u) ? totalLayers : std::min(baseLayer + layerCount, totalLayers);

        // All subresources? Collapse to uniform.
        if (baseMip == 0 && mipEnd >= totalMips && baseLayer == 0 && layerEnd >= totalLayers) {
            currentLayout = layout;
            m_subresourceLayouts.clear();
            return;
        }

        // Promote to per-subresource.
        if (m_subresourceLayouts.empty()) {
            if (layout == currentLayout) return;
            m_subresourceLayouts.resize(totalMips * totalLayers, currentLayout);
        }

        for (u32 l = baseLayer; l < layerEnd; ++l)
            for (u32 m = baseMip; m < mipEnd; ++m)
                m_subresourceLayouts[m + l * totalMips] = layout;

        // Try to collapse back to uniform.
        VkImageLayout first = m_subresourceLayouts[0];
        for (usize i = 1; i < m_subresourceLayouts.size(); ++i) {
            if (m_subresourceLayouts[i] != first) return;
        }
        currentLayout = first;
        m_subresourceLayouts.clear();
    }

private:
    VkImage        m_image     = VK_NULL_HANDLE;
    VkDeviceMemory m_memory    = VK_NULL_HANDLE;
    bool           m_ownsImage = true;
    std::vector<VkImageLayout> m_subresourceLayouts;
};

} // namespace draco::rhi::vk
