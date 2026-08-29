/// Vulkan implementation of ShaderModule.

module;

#include "VkIncludes.h"

export module rhi.vk:shader_module;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::vk {

class VkShaderModuleImpl : public ShaderModule {
public:
    Status init(VkDevice device, const ShaderModuleDesc& d) {
        VkShaderModuleCreateInfo ci{};
        ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = d.code.size();
        ci.pCode    = reinterpret_cast<const u32*>(d.code.data());

        if (vkCreateShaderModule(device, &ci, nullptr, &m_module) != VK_SUCCESS) return ErrorCode::Unknown;
        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device) {
        if (m_module != VK_NULL_HANDLE) { vkDestroyShaderModule(device, m_module, nullptr); m_module = VK_NULL_HANDLE; }
    }

    [[nodiscard]] VkShaderModule handle() const { return m_module; }

private:
    VkShaderModule m_module = VK_NULL_HANDLE;
};

} // namespace draco::rhi::vk
