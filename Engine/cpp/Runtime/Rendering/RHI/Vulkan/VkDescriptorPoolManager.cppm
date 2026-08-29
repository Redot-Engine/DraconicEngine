/// Manages VkDescriptorPool allocation with auto-grow.

module;

#include "VkIncludes.h"
#include <vector>


export module rhi.vk:descriptor_pool_manager;

import core.stdtypes;
import core.status;

using namespace draco;

export namespace draco::rhi::vk {

class VkDescriptorPoolManager {
public:
    VkDescriptorPoolManager(VkDevice device, u32 maxSetsPerPool = 256, bool accelStructEnabled = false)
        : m_device(device), m_maxSetsPerPool(maxSetsPerPool), m_accelStructEnabled(accelStructEnabled) {}

    Status allocate(VkDescriptorSetLayout layout, VkDescriptorPool& outPool,
                    bool updateAfterBind = false, u32 variableCount = 0) {
        outPool = VK_NULL_HANDLE;

        VkDescriptorSetVariableDescriptorCountAllocateInfo varCountInfo{};
        varCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        if (variableCount > 0) {
            varCountInfo.descriptorSetCount = 1;
            varCountInfo.pDescriptorCounts  = &variableCount;
        }

        // Try existing pools.
        for (auto pool : m_pools) {
            VkDescriptorSet set = VK_NULL_HANDLE;
            VkDescriptorSetAllocateInfo ai{};
            ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            ai.descriptorPool     = pool;
            ai.descriptorSetCount = 1;
            ai.pSetLayouts        = &layout;
            if (variableCount > 0) ai.pNext = &varCountInfo;

            if (vkAllocateDescriptorSets(m_device, &ai, &set) == VK_SUCCESS) {
                outPool = pool;
                m_lastAllocatedSet = set;
                return ErrorCode::Ok;
            }
        }

        // Create new pool.
        if (createPool(updateAfterBind) != ErrorCode::Ok) return ErrorCode::Unknown;

        auto pool = m_pools.back();
        VkDescriptorSet set = VK_NULL_HANDLE;
        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = pool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &layout;
        if (variableCount > 0) ai.pNext = &varCountInfo;

        if (vkAllocateDescriptorSets(m_device, &ai, &set) != VK_SUCCESS) return ErrorCode::Unknown;

        outPool = pool;
        m_lastAllocatedSet = set;
        return ErrorCode::Ok;
    }

    [[nodiscard]] VkDescriptorSet lastAllocatedSet() const { return m_lastAllocatedSet; }

    void free(VkDescriptorPool pool, VkDescriptorSet set) {
        vkFreeDescriptorSets(m_device, pool, 1, &set);
    }

    void destroy() {
        for (auto pool : m_pools) vkDestroyDescriptorPool(m_device, pool, nullptr);
        m_pools.clear();
    }

private:
    Status createPool(bool updateAfterBind) {
        u32 mult = updateAfterBind ? 64 : 1;
        VkDescriptorPoolSize sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER,                    m_maxSetsPerPool * mult },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              m_maxSetsPerPool * 4 * mult },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              m_maxSetsPerPool * mult },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             m_maxSetsPerPool * 2 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             m_maxSetsPerPool * 2 * mult },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,     m_maxSetsPerPool },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,     m_maxSetsPerPool },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     m_maxSetsPerPool * 4 * mult },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,           m_maxSetsPerPool },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,       m_maxSetsPerPool },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,       m_maxSetsPerPool },
            { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, m_maxSetsPerPool },
        };
        u32 sizeCount = m_accelStructEnabled ? 12 : 11;

        VkDescriptorPoolCreateInfo ci{};
        ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        if (updateAfterBind) ci.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        ci.maxSets       = m_maxSetsPerPool;
        ci.poolSizeCount = sizeCount;
        ci.pPoolSizes    = sizes;

        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(m_device, &ci, nullptr, &pool) != VK_SUCCESS) return ErrorCode::Unknown;
        m_pools.push_back(pool);
        return ErrorCode::Ok;
    }

    VkDevice                      m_device;
    std::vector<VkDescriptorPool> m_pools;
    u32                           m_maxSetsPerPool;
    bool                          m_accelStructEnabled;
    VkDescriptorSet               m_lastAllocatedSet = VK_NULL_HANDLE;
};

} // namespace draco::rhi::vk
