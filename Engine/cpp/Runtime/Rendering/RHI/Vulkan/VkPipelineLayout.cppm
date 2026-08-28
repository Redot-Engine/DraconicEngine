/// Vulkan implementation of PipelineLayout.

module;

#include "VkIncludes.h"
#include <vector>


export module rhi.vk:pipeline_layout;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :bind_group_layout;

using namespace draco;

export namespace draco::rhi::vk {

class VkPipelineLayoutImpl : public PipelineLayout {
public:
    Status init(VkDevice device, const PipelineLayoutDesc& desc) {
        std::vector<VkDescriptorSetLayout> setLayouts(desc.bindGroupLayouts.size());
        for (usize i = 0; i < desc.bindGroupLayouts.size(); ++i) {
            auto* vkl = static_cast<VkBindGroupLayoutImpl*>(desc.bindGroupLayouts[i]);
            if (!vkl) return ErrorCode::Unknown;
            setLayouts[i] = vkl->handle();
        }

        std::vector<VkPushConstantRange> pushRanges(desc.pushConstantRanges.size());
        for (usize i = 0; i < desc.pushConstantRanges.size(); ++i) {
            pushRanges[i] = {};
            pushRanges[i].stageFlags = toVkShaderStageFlags(desc.pushConstantRanges[i].stages);
            pushRanges[i].offset     = desc.pushConstantRanges[i].offset;
            pushRanges[i].size       = desc.pushConstantRanges[i].size;
        }

        VkPipelineLayoutCreateInfo ci{};
        ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.setLayoutCount         = static_cast<u32>(setLayouts.size());
        ci.pSetLayouts            = setLayouts.data();
        ci.pushConstantRangeCount = static_cast<u32>(pushRanges.size());
        ci.pPushConstantRanges    = pushRanges.data();

        if (vkCreatePipelineLayout(device, &ci, nullptr, &m_layout) != VK_SUCCESS) return ErrorCode::Unknown;
        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device) {
        if (m_layout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, m_layout, nullptr); m_layout = VK_NULL_HANDLE; }
    }

    [[nodiscard]] VkPipelineLayout handle() const { return m_layout; }

private:
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
};

} // namespace draco::rhi::vk
