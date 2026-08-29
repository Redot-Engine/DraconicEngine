/// Vulkan implementation of RenderPipeline.

module;

#include "VkIncludes.h"
#include <vector>
#include <span>
#include <string>


export module rhi.vk:render_pipeline;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :shader_module;
import :pipeline_layout;
import :pipeline_cache;

using namespace draco;

export namespace draco::rhi::vk {

class VkRenderPipelineImpl : public RenderPipeline {
public:
    Status init(VkDevice device, const RenderPipelineDesc& desc) {
        auto* vkLayout = static_cast<VkPipelineLayoutImpl*>(desc.layout);
        if (!vkLayout) return ErrorCode::Unknown;
        layout = desc.layout;
        m_layout = vkLayout;

        // Shader stages.
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        std::u8string vsEntry = std::u8string(desc.vertex.shader.entryPoint);
        std::u8string fsEntry;

        auto* vsMod = static_cast<VkShaderModuleImpl*>(desc.vertex.shader.module);
        if (!vsMod) return ErrorCode::Unknown;
        VkPipelineShaderStageCreateInfo vsStage{};
        vsStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vsStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vsStage.module = vsMod->handle();
        vsStage.pName  = reinterpret_cast<const char*>(vsEntry.c_str());
        stages.push_back(vsStage);

        if (desc.fragment.has_value()) {
            fsEntry = std::u8string(desc.fragment->shader.entryPoint);
            auto* fsMod = static_cast<VkShaderModuleImpl*>(desc.fragment->shader.module);
            if (fsMod) {
                VkPipelineShaderStageCreateInfo fsStage{};
                fsStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                fsStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
                fsStage.module = fsMod->handle();
                fsStage.pName  = reinterpret_cast<const char*>(fsEntry.c_str());
                stages.push_back(fsStage);
            }
        }

        // Vertex input.
        std::vector<VkVertexInputBindingDescription>   vertBindings;
        std::vector<VkVertexInputAttributeDescription> vertAttribs;
        for (usize i = 0; i < desc.vertex.buffers.size(); ++i) {
            const auto& buf = desc.vertex.buffers[i];
            VkVertexInputBindingDescription b{};
            b.binding   = static_cast<u32>(i);
            b.stride    = buf.stride;
            b.inputRate = buf.stepMode == VertexStepMode::Instance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
            vertBindings.push_back(b);
            for (usize j = 0; j < buf.attributes.size(); ++j) {
                VkVertexInputAttributeDescription a{};
                a.location = buf.attributes[j].shaderLocation;
                a.binding  = static_cast<u32>(i);
                a.format   = toVkVertexFormat(buf.attributes[j].format);
                a.offset   = buf.attributes[j].offset;
                vertAttribs.push_back(a);
            }
        }
        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = static_cast<u32>(vertBindings.size());
        vertexInput.pVertexBindingDescriptions       = vertBindings.data();
        vertexInput.vertexAttributeDescriptionCount = static_cast<u32>(vertAttribs.size());
        vertexInput.pVertexAttributeDescriptions     = vertAttribs.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = toVkTopology(desc.primitive.topology);

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.depthClampEnable = desc.primitive.depthClipEnabled ? VK_FALSE : VK_TRUE;
        rasterization.polygonMode      = toVkPolygonMode(desc.primitive.fillMode);
        rasterization.cullMode         = toVkCullMode(desc.primitive.cullMode);
        rasterization.frontFace        = toVkFrontFace(desc.primitive.frontFace);
        rasterization.lineWidth        = 1.0f;
        if (desc.depthStencil.has_value()) {
            rasterization.depthBiasEnable         = (desc.depthStencil->depthBias != 0 || desc.depthStencil->depthBiasSlopeScale != 0) ? VK_TRUE : VK_FALSE;
            rasterization.depthBiasConstantFactor = static_cast<f32>(desc.depthStencil->depthBias);
            rasterization.depthBiasSlopeFactor    = desc.depthStencil->depthBiasSlopeScale;
            rasterization.depthBiasClamp          = desc.depthStencil->depthBiasClamp;
        }

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples  = toVkSampleCount(desc.multisample.count);
        multisample.alphaToCoverageEnable = desc.multisample.alphaToCoverageEnabled ? VK_TRUE : VK_FALSE;
        u32 sampleMask = desc.multisample.mask;
        multisample.pSampleMask = &sampleMask;

        // Color blend.
        auto colorTargets = desc.fragment.has_value() ? desc.fragment->targets : std::span<const ColorTargetState>();
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(colorTargets.size());
        for (usize i = 0; i < colorTargets.size(); ++i) {
            auto& ba = blendAttachments[i];
            ba = {};
            ba.colorWriteMask = toVkColorWriteMask(colorTargets[i].writeMask);
            if (colorTargets[i].blend.has_value()) {
                ba.blendEnable         = VK_TRUE;
                ba.srcColorBlendFactor = toVkBlendFactor(colorTargets[i].blend->color.srcFactor);
                ba.dstColorBlendFactor = toVkBlendFactor(colorTargets[i].blend->color.dstFactor);
                ba.colorBlendOp        = toVkBlendOp(colorTargets[i].blend->color.operation);
                ba.srcAlphaBlendFactor = toVkBlendFactor(colorTargets[i].blend->alpha.srcFactor);
                ba.dstAlphaBlendFactor = toVkBlendFactor(colorTargets[i].blend->alpha.dstFactor);
                ba.alphaBlendOp        = toVkBlendOp(colorTargets[i].blend->alpha.operation);
            }
        }
        VkPipelineColorBlendStateCreateInfo colorBlend{};
        colorBlend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlend.attachmentCount = static_cast<u32>(blendAttachments.size());
        colorBlend.pAttachments    = blendAttachments.data();

        // Depth/stencil.
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        if (desc.depthStencil.has_value()) {
            const auto& ds = *desc.depthStencil;
            depthStencil.depthTestEnable   = ds.depthTestEnabled  ? VK_TRUE : VK_FALSE;
            depthStencil.depthWriteEnable  = ds.depthWriteEnabled ? VK_TRUE : VK_FALSE;
            depthStencil.depthCompareOp    = toVkCompareOp(ds.depthCompare);
            depthStencil.stencilTestEnable = ds.stencilEnabled    ? VK_TRUE : VK_FALSE;
            depthStencil.front.failOp      = toVkStencilOp(ds.stencilFront.failOp);
            depthStencil.front.passOp      = toVkStencilOp(ds.stencilFront.passOp);
            depthStencil.front.depthFailOp = toVkStencilOp(ds.stencilFront.depthFailOp);
            depthStencil.front.compareOp   = toVkCompareOp(ds.stencilFront.compare);
            depthStencil.front.compareMask = ds.stencilReadMask;
            depthStencil.front.writeMask   = ds.stencilWriteMask;
            depthStencil.back.failOp       = toVkStencilOp(ds.stencilBack.failOp);
            depthStencil.back.passOp       = toVkStencilOp(ds.stencilBack.passOp);
            depthStencil.back.depthFailOp  = toVkStencilOp(ds.stencilBack.depthFailOp);
            depthStencil.back.compareOp    = toVkCompareOp(ds.stencilBack.compare);
            depthStencil.back.compareMask  = ds.stencilReadMask;
            depthStencil.back.writeMask    = ds.stencilWriteMask;
        }

        // Dynamic state.
        VkDynamicState dynStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
            VK_DYNAMIC_STATE_BLEND_CONSTANTS, VK_DYNAMIC_STATE_STENCIL_REFERENCE
        };
        VkPipelineDynamicStateCreateInfo dynState{};
        dynState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynState.dynamicStateCount = 4;
        dynState.pDynamicStates    = dynStates;

        // Dynamic rendering (Vulkan 1.3).
        std::vector<VkFormat> colorFormats(colorTargets.size());
        for (usize i = 0; i < colorTargets.size(); ++i)
            colorFormats[i] = toVkFormat(colorTargets[i].format);

        VkPipelineRenderingCreateInfo renderingInfo{};
        renderingInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        renderingInfo.colorAttachmentCount    = static_cast<u32>(colorFormats.size());
        renderingInfo.pColorAttachmentFormats = colorFormats.data();
        if (desc.depthStencil.has_value()) {
            VkFormat dsf = toVkFormat(desc.depthStencil->format);
            if (hasDepth(desc.depthStencil->format))   renderingInfo.depthAttachmentFormat   = dsf;
            if (hasStencil(desc.depthStencil->format)) renderingInfo.stencilAttachmentFormat = dsf;
        }

        // Create pipeline.
        VkGraphicsPipelineCreateInfo pi{};
        pi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pi.pNext               = &renderingInfo;
        pi.stageCount          = static_cast<u32>(stages.size());
        pi.pStages             = stages.data();
        pi.pVertexInputState   = &vertexInput;
        pi.pInputAssemblyState = &inputAssembly;
        pi.pViewportState      = &viewportState;
        pi.pRasterizationState = &rasterization;
        pi.pMultisampleState   = &multisample;
        pi.pDepthStencilState  = desc.depthStencil.has_value() ? &depthStencil : nullptr;
        pi.pColorBlendState    = &colorBlend;
        pi.pDynamicState       = &dynState;
        pi.layout              = vkLayout->handle();

        VkPipelineCache cacheHandle = VK_NULL_HANDLE;
        if (desc.cache) cacheHandle = static_cast<VkPipelineCacheImpl*>(desc.cache)->handle();

        if (vkCreateGraphicsPipelines(device, cacheHandle, 1, &pi, nullptr, &m_pipeline) != VK_SUCCESS)
            return ErrorCode::Unknown;
        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device) {
        if (m_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(device, m_pipeline, nullptr); m_pipeline = VK_NULL_HANDLE; }
    }

    [[nodiscard]] VkPipeline          handle()       const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayoutImpl* vkLayout()    const { return m_layout; }

private:
    VkPipeline           m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayoutImpl* m_layout  = nullptr;
};

} // namespace draco::rhi::vk
