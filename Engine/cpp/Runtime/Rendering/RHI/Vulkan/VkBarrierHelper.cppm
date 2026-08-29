/// Converts ResourceState to Vulkan synchronization2 stage/access/layout.

module;

#include "VkIncludes.h"

export module rhi.vk:barrier_helper;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::vk {

struct StageAccess {
    u64 stageMask  = 0;
    u64 accessMask = 0;
};

inline StageAccess getStageAccess(ResourceState state) {
    StageAccess sa{};
    auto s = static_cast<u32>(state);
    auto has = [&](ResourceState f) { return (s & static_cast<u32>(f)) != 0; };

    if (has(ResourceState::VertexBuffer))     { sa.stageMask |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT; sa.accessMask |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT; }
    if (has(ResourceState::IndexBuffer))      { sa.stageMask |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT; sa.accessMask |= VK_ACCESS_2_INDEX_READ_BIT; }
    if (has(ResourceState::UniformBuffer))    { sa.stageMask |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT; sa.accessMask |= VK_ACCESS_2_UNIFORM_READ_BIT; }
    if (has(ResourceState::ShaderRead))       { sa.stageMask |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT; sa.accessMask |= VK_ACCESS_2_SHADER_READ_BIT; }
    if (has(ResourceState::ShaderWrite))      { sa.stageMask |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT; sa.accessMask |= VK_ACCESS_2_SHADER_WRITE_BIT; }
    if (has(ResourceState::RenderTarget))     { sa.stageMask |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT; sa.accessMask |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT; }
    if (has(ResourceState::DepthStencilWrite)){ sa.stageMask |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT; sa.accessMask |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT; }
    if (has(ResourceState::DepthStencilRead)) { sa.stageMask |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT; sa.accessMask |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT; }
    if (has(ResourceState::IndirectArgument)) { sa.stageMask |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT; sa.accessMask |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT; }
    if (has(ResourceState::CopySrc))          { sa.stageMask |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT; sa.accessMask |= VK_ACCESS_2_TRANSFER_READ_BIT; }
    if (has(ResourceState::CopyDst))          { sa.stageMask |= VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT; sa.accessMask |= VK_ACCESS_2_TRANSFER_WRITE_BIT; }
    if (has(ResourceState::Present))          { sa.stageMask |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT; }
    if (has(ResourceState::AccelStructRead))  { sa.stageMask |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR; sa.accessMask |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR; }
    if (has(ResourceState::AccelStructWrite)) { sa.stageMask |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR; sa.accessMask |= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR; }
    if (has(ResourceState::General))          { sa.stageMask |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; sa.accessMask |= VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT; }

    if (sa.stageMask == 0) sa.stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    return sa;
}

inline VkImageLayout getImageLayout(ResourceState state, TextureFormat format = TextureFormat::Undefined) {
    auto s = static_cast<u32>(state);
    auto has = [&](ResourceState f) { return (s & static_cast<u32>(f)) != 0; };

    if (has(ResourceState::Present))          return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    if (has(ResourceState::RenderTarget))     return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (has(ResourceState::DepthStencilWrite))return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    if (has(ResourceState::DepthStencilRead)) return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    if (has(ResourceState::ShaderRead)) {
        // Depth/stencil textures must use DEPTH_STENCIL_READ_ONLY when sampled,
        // not SHADER_READ_ONLY - the latter is only for color textures.
        if (isDepthFormat(format))
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    if (has(ResourceState::ShaderWrite))      return VK_IMAGE_LAYOUT_GENERAL;
    if (has(ResourceState::CopySrc))          return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if (has(ResourceState::CopyDst))          return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    if (has(ResourceState::General))          return VK_IMAGE_LAYOUT_GENERAL;
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

} // namespace draco::rhi::vk
