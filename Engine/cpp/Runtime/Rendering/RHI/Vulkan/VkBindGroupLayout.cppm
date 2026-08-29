/// Vulkan implementation of BindGroupLayout.

module;

#include "VkIncludes.h"
#include <vector>
#include <span>


export module rhi.vk:bind_group_layout;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :binding_shifts;

using namespace draco;

export namespace draco::rhi::vk {

inline VkDescriptorType toVkDescriptorType(const BindGroupLayoutEntry& e) {
    switch (e.type) {
    case BindingType::UniformBuffer:
        return e.hasDynamicOffset ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case BindingType::StorageBufferReadOnly:
    case BindingType::StorageBufferReadWrite:
        return e.hasDynamicOffset ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case BindingType::SampledTexture:          return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case BindingType::StorageTextureReadOnly:
    case BindingType::StorageTextureReadWrite: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case BindingType::Sampler:
    case BindingType::ComparisonSampler:       return VK_DESCRIPTOR_TYPE_SAMPLER;
    case BindingType::BindlessTextures:        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case BindingType::BindlessSamplers:         return VK_DESCRIPTOR_TYPE_SAMPLER;
    case BindingType::BindlessStorageBuffers:   return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case BindingType::BindlessStorageTextures:  return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case BindingType::AccelerationStructure:   return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    }
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

class VkBindGroupLayoutImpl : public BindGroupLayout {
public:
    Status init(VkDevice device, const BindGroupLayoutDesc& desc, const BindingShifts& shifts = {}) {
        m_entries.clear();
        for (usize i = 0; i < desc.entries.size(); ++i) { m_entries.push_back(desc.entries[i]); }

        std::vector<VkDescriptorSetLayoutBinding> bindings(desc.entries.size());
        std::vector<VkDescriptorBindingFlags>     flags(desc.entries.size());

        for (usize i = 0; i < desc.entries.size(); ++i) {
            const auto& e = desc.entries[i];
            auto& b = bindings[i];
            b = {};
            b.binding         = shifts.apply(e.type, e.binding);
            b.descriptorType  = toVkDescriptorType(e);
            b.descriptorCount = e.count;
            b.stageFlags      = toVkShaderStageFlags(e.visibility);

            flags[i] = 0;
            if (e.count == ~0u) {
                b.descriptorCount = 1024 * 16;
                m_hasBindless      = true;
                m_bindlessCount    = b.descriptorCount;
                flags[i] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                         | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                         | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
            }
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
        flagsInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flagsInfo.bindingCount  = static_cast<u32>(desc.entries.size());
        flagsInfo.pBindingFlags = flags.data();

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = static_cast<u32>(desc.entries.size());
        ci.pBindings    = bindings.data();
        if (m_hasBindless) {
            ci.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
            ci.pNext  = &flagsInfo;
        }

        if (vkCreateDescriptorSetLayout(device, &ci, nullptr, &m_layout) != VK_SUCCESS)
            return ErrorCode::Unknown;
        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device) {
        if (m_layout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(device, m_layout, nullptr); m_layout = VK_NULL_HANDLE; }
    }

    std::span<const BindGroupLayoutEntry> entries() const override {
        return std::span<const BindGroupLayoutEntry>(m_entries.data(), m_entries.size());
    }

    [[nodiscard]] VkDescriptorSetLayout handle() const { return m_layout; }
    [[nodiscard]] bool hasBindless()   const { return m_hasBindless; }
    [[nodiscard]] u32  bindlessCount() const { return m_bindlessCount; }

private:
    VkDescriptorSetLayout                m_layout = VK_NULL_HANDLE;
    std::vector<BindGroupLayoutEntry>    m_entries;
    bool                                 m_hasBindless  = false;
    u32                                  m_bindlessCount = 0;
};

} // namespace draco::rhi::vk
