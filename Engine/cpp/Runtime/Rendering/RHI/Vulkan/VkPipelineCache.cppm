/// Vulkan implementation of PipelineCache.

module;

#include "VkIncludes.h"
#include <span>

export module rhi.vk:pipeline_cache;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::vk {

class VkPipelineCacheImpl : public PipelineCache {
public:
    Status init(VkDevice device, const PipelineCacheDesc& desc) {
        m_device = device;

        VkPipelineCacheCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        if (desc.initialData.size() > 0) {
            ci.initialDataSize = desc.initialData.size();
            ci.pInitialData    = desc.initialData.data();
        }

        if (vkCreatePipelineCache(device, &ci, nullptr, &m_cache) != VK_SUCCESS) return ErrorCode::Unknown;
        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device) {
        if (m_cache != VK_NULL_HANDLE) { vkDestroyPipelineCache(device, m_cache, nullptr); m_cache = VK_NULL_HANDLE; }
    }

    u32 getDataSize() override {
        usize size = 0;
        vkGetPipelineCacheData(m_device, m_cache, &size, nullptr);
        return static_cast<u32>(size);
    }

    Status getData(std::span<u8> outData) override {
        usize size = outData.size();
        if (vkGetPipelineCacheData(m_device, m_cache, &size, outData.data()) != VK_SUCCESS) return ErrorCode::Unknown;
        return ErrorCode::Ok;
    }

    [[nodiscard]] VkPipelineCache handle() const { return m_cache; }

private:
    VkPipelineCache m_cache  = VK_NULL_HANDLE;
    VkDevice        m_device = VK_NULL_HANDLE;
};

} // namespace draco::rhi::vk
