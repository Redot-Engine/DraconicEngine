/// Vulkan implementation of ComputePipeline.

module;

#include "VkIncludes.h"
#include <string>


export module rhi.vk:compute_pipeline;

import core.stdtypes;
import core.status;
import rhi;
import :shader_module;
import :pipeline_layout;
import :pipeline_cache;

using namespace draco;

export namespace draco::rhi::vk {

class VkComputePipelineImpl : public ComputePipeline {
public:
    Status init(VkDevice device, const ComputePipelineDesc& desc) {
        auto* vkLayout = static_cast<VkPipelineLayoutImpl*>(desc.layout);
        if (!vkLayout) return ErrorCode::Unknown;
        layout  = desc.layout;
        m_layout = vkLayout;

        auto* vkMod = static_cast<VkShaderModuleImpl*>(desc.compute.module);
        if (!vkMod) return ErrorCode::Unknown;

        std::u8string entry = std::u8string(desc.compute.entryPoint);

        VkPipelineShaderStageCreateInfo stage{};
        stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = vkMod->handle();
        stage.pName  = reinterpret_cast<const char*>(entry.c_str());

        VkComputePipelineCreateInfo ci{};
        ci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        ci.stage  = stage;
        ci.layout = vkLayout->handle();

        VkPipelineCache cacheHandle = VK_NULL_HANDLE;
        if (desc.cache) cacheHandle = static_cast<VkPipelineCacheImpl*>(desc.cache)->handle();

        if (vkCreateComputePipelines(device, cacheHandle, 1, &ci, nullptr, &m_pipeline) != VK_SUCCESS)
            return ErrorCode::Unknown;
        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device) {
        if (m_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, m_pipeline, nullptr); m_pipeline = VK_NULL_HANDLE; }
    }

    [[nodiscard]] VkPipeline           handle()   const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayoutImpl* vkLayout() const { return m_layout; }

private:
    VkPipeline            m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayoutImpl* m_layout   = nullptr;
};

} // namespace draco::rhi::vk
