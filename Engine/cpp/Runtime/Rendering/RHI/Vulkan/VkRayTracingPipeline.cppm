/// Vulkan implementation of RayTracingPipeline.

module;

#include "VkIncludes.h"
#include <vector>
#include <string>


export module rhi.vk:ray_tracing_pipeline;

import core.stdtypes;
import core.status;
import rhi;
import :shader_module;
import :pipeline_layout;
import :pipeline_cache;

using namespace draco;

export namespace draco::rhi::vk {

class VkRayTracingPipelineImpl : public RayTracingPipeline {
public:
    Status init(VkDevice device, const RayTracingPipelineDesc& desc) {
        auto* vkLayout = static_cast<VkPipelineLayoutImpl*>(desc.layout);
        if (!vkLayout) return ErrorCode::Unknown;
        layout  = desc.layout;
        m_layout = vkLayout;

        // Shader stages.
        std::vector<VkPipelineShaderStageCreateInfo> stages(desc.stages.size());
        std::vector<std::u8string> entryStrings(desc.stages.size());
        for (usize i = 0; i < desc.stages.size(); ++i) {
            entryStrings[i] = std::u8string(desc.stages[i].entryPoint);
            auto* mod = static_cast<VkShaderModuleImpl*>(desc.stages[i].module);
            if (!mod) return ErrorCode::Unknown;
            stages[i] = {};
            stages[i].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[i].module = mod->handle();
            stages[i].pName  = reinterpret_cast<const char*>(entryStrings[i].c_str());
            // Map stage from desc.
            auto s = desc.stages[i].stage;
            if      (s == ShaderStage::RayGen)       stages[i].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            else if (s == ShaderStage::Miss)         stages[i].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
            else if (s == ShaderStage::ClosestHit)   stages[i].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            else if (s == ShaderStage::AnyHit)       stages[i].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
            else if (s == ShaderStage::Intersection) stages[i].stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
            else if (s == ShaderStage::Callable)     stages[i].stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR;
            else                                     stages[i].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        }

        // Shader groups.
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups(desc.groups.size());
        for (usize i = 0; i < desc.groups.size(); ++i) {
            const auto& g = desc.groups[i];
            groups[i] = {};
            groups[i].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            switch (g.type) {
            case RayTracingShaderGroup::Type::General:
                groups[i].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR; break;
            case RayTracingShaderGroup::Type::TrianglesHitGroup:
                groups[i].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR; break;
            case RayTracingShaderGroup::Type::ProceduralHitGroup:
                groups[i].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR; break;
            }
            groups[i].generalShader      = g.generalShaderIndex;
            groups[i].closestHitShader   = g.closestHitShaderIndex;
            groups[i].anyHitShader       = g.anyHitShaderIndex;
            groups[i].intersectionShader = g.intersectionShaderIndex;
        }

        VkRayTracingPipelineCreateInfoKHR ci{};
        ci.sType             = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        ci.stageCount        = static_cast<u32>(stages.size());
        ci.pStages           = stages.data();
        ci.groupCount        = static_cast<u32>(groups.size());
        ci.pGroups           = groups.data();
        ci.maxPipelineRayRecursionDepth = desc.maxRecursionDepth;
        ci.layout            = vkLayout->handle();

        auto pfn = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
            vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
        if (!pfn) return ErrorCode::Unknown;

        VkPipelineCache cacheHandle = VK_NULL_HANDLE;
        if (desc.cache) cacheHandle = static_cast<VkPipelineCacheImpl*>(desc.cache)->handle();

        if (pfn(device, VK_NULL_HANDLE, cacheHandle, 1, &ci, nullptr, &m_pipeline) != VK_SUCCESS)
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
