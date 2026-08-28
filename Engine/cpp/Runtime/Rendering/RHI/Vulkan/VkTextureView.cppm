/// Vulkan implementation of TextureView.

module;

#include "VkIncludes.h"

export module rhi.vk:texture_view;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :texture;

using namespace draco;

export namespace draco::rhi::vk {

class VkTextureViewImpl : public TextureView {
public:
    Status init(VkDevice device, VkTextureImpl* tex, const TextureViewDesc& d) {
        desc    = d;
        texture = tex;
        // Dimensions are the view's BASE MIP extent, not the texture's mip-0 size - a view onto mip N
        // is half-sized per level. Render-pass renderArea/framebuffer is derived from these, so a stale
        // mip-0 size here overruns a mip>0 attachment (GPU fault). Shadows only ever target mip 0; the
        // IBL prefilter is the first mip>0 render target to exercise this.
        m_width  = tex->desc.width  >> d.baseMipLevel; if (m_width  == 0) { m_width  = 1; }
        m_height = tex->desc.height >> d.baseMipLevel; if (m_height == 0) { m_height = 1; }

        TextureFormat fmt = (d.format == TextureFormat::Undefined) ? tex->desc.format : d.format;
        m_format = fmt;

        VkImageViewCreateInfo ci{};
        ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image    = tex->handle();
        ci.viewType = toVkImageViewType(d.dimension);
        ci.format   = toVkFormat(fmt);
        ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };

        u32 mipCount   = d.mipLevelCount   > 0 ? d.mipLevelCount   : tex->desc.mipLevelCount   - d.baseMipLevel;
        u32 layerCount = d.arrayLayerCount  > 0 ? d.arrayLayerCount : tex->desc.arrayLayerCount - d.baseArrayLayer;

        VkImageAspectFlags aspect;
        switch (d.aspect) {
        case TextureAspect::DepthOnly:   aspect = VK_IMAGE_ASPECT_DEPTH_BIT; break;
        case TextureAspect::StencilOnly: aspect = VK_IMAGE_ASPECT_STENCIL_BIT; break;
        default:                         aspect = getAspectMask(fmt); break;
        }

        ci.subresourceRange = { aspect, d.baseMipLevel, mipCount, d.baseArrayLayer, layerCount };

        if (vkCreateImageView(device, &ci, nullptr, &m_imageView) != VK_SUCCESS) return ErrorCode::Unknown;
        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device) {
        if (m_imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, m_imageView, nullptr);
            m_imageView = VK_NULL_HANDLE;
        }
    }

    [[nodiscard]] VkImageView   handle() const { return m_imageView; }
    [[nodiscard]] TextureFormat format() const { return m_format; }
    [[nodiscard]] u32           width()  const { return m_width; }
    [[nodiscard]] u32           height() const { return m_height; }

private:
    VkImageView   m_imageView = VK_NULL_HANDLE;
    TextureFormat m_format    = TextureFormat::Undefined;
    u32           m_width     = 0;
    u32           m_height    = 0;
};

} // namespace draco::rhi::vk
