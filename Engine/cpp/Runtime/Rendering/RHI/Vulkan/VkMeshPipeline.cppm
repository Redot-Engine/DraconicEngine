/// Vulkan implementation of MeshPipeline.

module;

#include "VkIncludes.h"
#include <vector>
#include <string>


export module rhi.vk:mesh_pipeline;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :shader_module;
import :pipeline_layout;
import :pipeline_cache;

using namespace draco;

export namespace draco::rhi::vk {

class VkMeshPipelineImpl : public MeshPipeline {
public:
    Status init(VkDevice device, const MeshPipelineDesc& desc) {
        auto* vkLayout = static_cast<VkPipelineLayoutImpl*>(desc.layout);
        if (!vkLayout) return ErrorCode::Unknown;
        layout  = desc.layout;
        m_layout = vkLayout;

        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::u8string meshEntry = std::u8string(desc.mesh.entryPoint);
        std::u8string taskEntry, fsEntry;

        // Task shader (optional).
        if (desc.task.has_value()) {
            taskEntry = std::u8string(desc.task->entryPoint);
            if (auto* mod = static_cast<VkShaderModuleImpl*>(desc.task->module)) {
                VkPipelineShaderStageCreateInfo s{}; s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                s.stage = VK_SHADER_STAGE_TASK_BIT_EXT; s.module = mod->handle(); s.pName = reinterpret_cast<const char*>(taskEntry.c_str());
                stages.push_back(s);
            }
        }

        // Mesh shader (required).
        auto* meshMod = static_cast<VkShaderModuleImpl*>(desc.mesh.module);
        if (!meshMod) return ErrorCode::Unknown;
        { VkPipelineShaderStageCreateInfo s{}; s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
          s.stage = VK_SHADER_STAGE_MESH_BIT_EXT; s.module = meshMod->handle(); s.pName = reinterpret_cast<const char*>(meshEntry.c_str());
          stages.push_back(s); }

        // Fragment shader (optional).
        if (desc.fragment.has_value()) {
            fsEntry = std::u8string(desc.fragment->shader.entryPoint);
            if (auto* mod = static_cast<VkShaderModuleImpl*>(desc.fragment->shader.module)) {
                VkPipelineShaderStageCreateInfo s{}; s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                s.stage = VK_SHADER_STAGE_FRAGMENT_BIT; s.module = mod->handle(); s.pName = reinterpret_cast<const char*>(fsEntry.c_str());
                stages.push_back(s);
            }
        }

        // Viewport (dynamic).
        VkPipelineViewportStateCreateInfo vp{}; vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.viewportCount = 1; vp.scissorCount = 1;

        // Rasterization.
        VkPipelineRasterizationStateCreateInfo rs{}; rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.depthClampEnable = desc.primitive.depthClipEnabled ? VK_FALSE : VK_TRUE;
        rs.polygonMode = toVkPolygonMode(desc.primitive.fillMode);
        rs.cullMode    = toVkCullMode(desc.primitive.cullMode);
        rs.frontFace   = toVkFrontFace(desc.primitive.frontFace);
        rs.lineWidth   = 1.0f;
        if (desc.depthStencil.has_value()) {
            rs.depthBiasEnable         = (desc.depthStencil->depthBias != 0 || desc.depthStencil->depthBiasSlopeScale != 0) ? VK_TRUE : VK_FALSE;
            rs.depthBiasConstantFactor = static_cast<f32>(desc.depthStencil->depthBias);
            rs.depthBiasSlopeFactor    = desc.depthStencil->depthBiasSlopeScale;
            rs.depthBiasClamp          = desc.depthStencil->depthBiasClamp;
        }

        // Multisample.
        VkPipelineMultisampleStateCreateInfo ms{}; ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples  = toVkSampleCount(desc.multisample.count);
        ms.alphaToCoverageEnable = desc.multisample.alphaToCoverageEnabled ? VK_TRUE : VK_FALSE;
        u32 sampleMask = desc.multisample.mask; ms.pSampleMask = &sampleMask;

        // Color blend.
        std::vector<VkPipelineColorBlendAttachmentState> blendAtts(desc.colorTargets.size());
        for (usize i = 0; i < desc.colorTargets.size(); ++i) {
            auto& ba = blendAtts[i]; ba = {};
            ba.colorWriteMask = toVkColorWriteMask(desc.colorTargets[i].writeMask);
            if (desc.colorTargets[i].blend.has_value()) {
                ba.blendEnable = VK_TRUE;
                ba.srcColorBlendFactor = toVkBlendFactor(desc.colorTargets[i].blend->color.srcFactor);
                ba.dstColorBlendFactor = toVkBlendFactor(desc.colorTargets[i].blend->color.dstFactor);
                ba.colorBlendOp        = toVkBlendOp(desc.colorTargets[i].blend->color.operation);
                ba.srcAlphaBlendFactor = toVkBlendFactor(desc.colorTargets[i].blend->alpha.srcFactor);
                ba.dstAlphaBlendFactor = toVkBlendFactor(desc.colorTargets[i].blend->alpha.dstFactor);
                ba.alphaBlendOp        = toVkBlendOp(desc.colorTargets[i].blend->alpha.operation);
            }
        }
        VkPipelineColorBlendStateCreateInfo cb{}; cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = static_cast<u32>(blendAtts.size()); cb.pAttachments = blendAtts.data();

        // Depth/stencil.
        VkPipelineDepthStencilStateCreateInfo dss{}; dss.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        if (desc.depthStencil.has_value()) {
            const auto& ds = *desc.depthStencil;
            dss.depthTestEnable   = ds.depthTestEnabled  ? VK_TRUE : VK_FALSE;
            dss.depthWriteEnable  = ds.depthWriteEnabled ? VK_TRUE : VK_FALSE;
            dss.depthCompareOp    = toVkCompareOp(ds.depthCompare);
            dss.stencilTestEnable = ds.stencilEnabled    ? VK_TRUE : VK_FALSE;
            dss.front.failOp = toVkStencilOp(ds.stencilFront.failOp); dss.front.passOp = toVkStencilOp(ds.stencilFront.passOp);
            dss.front.depthFailOp = toVkStencilOp(ds.stencilFront.depthFailOp); dss.front.compareOp = toVkCompareOp(ds.stencilFront.compare);
            dss.front.compareMask = ds.stencilReadMask; dss.front.writeMask = ds.stencilWriteMask;
            dss.back = dss.front;
            dss.back.failOp = toVkStencilOp(ds.stencilBack.failOp); dss.back.passOp = toVkStencilOp(ds.stencilBack.passOp);
            dss.back.depthFailOp = toVkStencilOp(ds.stencilBack.depthFailOp); dss.back.compareOp = toVkCompareOp(ds.stencilBack.compare);
        }

        // Dynamic state.
        VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                                       VK_DYNAMIC_STATE_BLEND_CONSTANTS, VK_DYNAMIC_STATE_STENCIL_REFERENCE };
        VkPipelineDynamicStateCreateInfo dyn{}; dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 4; dyn.pDynamicStates = dynStates;

        // Dynamic rendering.
        std::vector<VkFormat> colorFmts(desc.colorTargets.size());
        for (usize i = 0; i < desc.colorTargets.size(); ++i) colorFmts[i] = toVkFormat(desc.colorTargets[i].format);
        VkPipelineRenderingCreateInfo ri{}; ri.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        ri.colorAttachmentCount = static_cast<u32>(colorFmts.size()); ri.pColorAttachmentFormats = colorFmts.data();
        if (desc.depthStencil.has_value()) {
            VkFormat dsf = toVkFormat(desc.depthStencil->format);
            if (hasDepth(desc.depthStencil->format))   ri.depthAttachmentFormat   = dsf;
            if (hasStencil(desc.depthStencil->format)) ri.stencilAttachmentFormat = dsf;
        }

        // Create pipeline (no vertex input / input assembly for mesh shaders).
        VkGraphicsPipelineCreateInfo pi{}; pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pi.pNext = &ri; pi.stageCount = static_cast<u32>(stages.size()); pi.pStages = stages.data();
        pi.pVertexInputState = nullptr; pi.pInputAssemblyState = nullptr;
        pi.pViewportState = &vp; pi.pRasterizationState = &rs; pi.pMultisampleState = &ms;
        pi.pDepthStencilState = desc.depthStencil.has_value() ? &dss : nullptr;
        pi.pColorBlendState = &cb; pi.pDynamicState = &dyn; pi.layout = vkLayout->handle();

        VkPipelineCache cacheHandle = VK_NULL_HANDLE;
        if (desc.cache) cacheHandle = static_cast<VkPipelineCacheImpl*>(desc.cache)->handle();

        if (vkCreateGraphicsPipelines(device, cacheHandle, 1, &pi, nullptr, &m_pipeline) != VK_SUCCESS)
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
