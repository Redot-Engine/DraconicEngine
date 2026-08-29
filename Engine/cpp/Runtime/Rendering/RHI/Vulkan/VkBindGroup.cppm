/// Vulkan implementation of BindGroup.

module;

#include "VkIncludes.h"
#include <vector>
#include <span>


export module rhi.vk:bind_group;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :binding_shifts;
import :bind_group_layout;
import :buffer;
import :texture;
import :texture_view;
import :sampler;
import :accel_struct;
import :descriptor_pool_manager;

using namespace draco;

export namespace draco::rhi::vk {

class VkBindGroupImpl : public BindGroup {
public:
    Status init(VkDevice device, VkDescriptorPoolManager* poolMgr, const BindGroupDesc& desc,
                 const BindingShifts& shifts = {}) {
        m_device = device;
        m_shifts = shifts;
        m_layout = static_cast<VkBindGroupLayoutImpl*>(desc.layout);
        if (!m_layout) return ErrorCode::Unknown;

        VkDescriptorPool pool;
        if (poolMgr->allocate(m_layout->handle(), pool, m_layout->hasBindless(), m_layout->bindlessCount()) != ErrorCode::Ok)
            return ErrorCode::Unknown;
        m_pool = pool;
        m_set  = poolMgr->lastAllocatedSet();

        writeDescriptors(device, desc);
        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device, VkDescriptorPoolManager* poolMgr) {
        if (m_set != VK_NULL_HANDLE && m_pool != VK_NULL_HANDLE) {
            poolMgr->free(m_pool, m_set);
            m_set  = VK_NULL_HANDLE;
            m_pool = VK_NULL_HANDLE;
        }
        (void)device;
    }

    BindGroupLayout* layout() override { return m_layout; }

    void updateBindless(std::span<const BindlessUpdateEntry> entries) override {
        if (entries.size() == 0) return;

        std::vector<VkWriteDescriptorSet>    writes;
        std::vector<VkDescriptorBufferInfo>  bufInfos;
        std::vector<VkDescriptorImageInfo>   imgInfos;
        bufInfos.reserve(entries.size());
        imgInfos.reserve(entries.size());

        auto layoutEntries = m_layout->entries();

        for (usize i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            if (e.layoutIndex >= static_cast<u32>(layoutEntries.size())) continue;
            const auto& le = layoutEntries[e.layoutIndex];

            VkWriteDescriptorSet w{};
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = m_set;
            w.dstBinding      = m_shifts.apply(le.type, le.binding);
            w.dstArrayElement = e.arrayIndex;
            w.descriptorCount = 1;
            w.descriptorType  = toVkDescriptorType(le);

            switch (le.type) {
            case BindingType::BindlessStorageBuffers:
                if (auto* vkBuf = static_cast<VkBufferImpl*>(e.buffer)) {
                    VkDescriptorBufferInfo bi{}; bi.buffer = vkBuf->handle(); bi.offset = e.bufferOffset;
                    bi.range = e.bufferSize > 0 ? e.bufferSize : VK_WHOLE_SIZE;
                    bufInfos.push_back(bi); w.pBufferInfo = &bufInfos.back();
                } else continue;
                break;
            case BindingType::BindlessTextures: case BindingType::BindlessStorageTextures:
                if (auto* vkView = static_cast<VkTextureViewImpl*>(e.textureView)) {
                    VkDescriptorImageInfo ii{}; ii.imageView = vkView->handle();
                    ii.imageLayout = le.type == BindingType::BindlessTextures
                        ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
                    imgInfos.push_back(ii); w.pImageInfo = &imgInfos.back();
                } else continue;
                break;
            case BindingType::BindlessSamplers:
                if (auto* vkSamp = static_cast<VkSamplerImpl*>(e.sampler)) {
                    VkDescriptorImageInfo ii{}; ii.sampler = vkSamp->handle();
                    imgInfos.push_back(ii); w.pImageInfo = &imgInfos.back();
                } else continue;
                break;
            default: continue;
            }
            writes.push_back(w);
        }

        if (!writes.empty())
            vkUpdateDescriptorSets(m_device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    [[nodiscard]] VkDescriptorSet handle() const { return m_set; }

private:
    void writeDescriptors(VkDevice device, const BindGroupDesc& desc) {
        if (desc.entries.size() == 0) return;

        auto layoutEntries = m_layout->entries();

        std::vector<VkWriteDescriptorSet>    writes;
        std::vector<VkDescriptorBufferInfo>  bufInfos;
        std::vector<VkDescriptorImageInfo>   imgInfos;
        std::vector<VkWriteDescriptorSetAccelerationStructureKHR> asWriteInfos;
        std::vector<VkAccelerationStructureKHR>                   asHandles;
        bufInfos.reserve(desc.entries.size());
        imgInfos.reserve(desc.entries.size());
        asWriteInfos.reserve(desc.entries.size());
        asHandles.reserve(desc.entries.size());

        usize entryIdx = 0;
        for (usize i = 0; i < layoutEntries.size(); ++i) {
            const auto& le = layoutEntries[i];
            // Skip bindless entries.
            switch (le.type) {
            case BindingType::BindlessTextures: case BindingType::BindlessSamplers:
            case BindingType::BindlessStorageBuffers: case BindingType::BindlessStorageTextures:
                continue;
            default: break;
            }
            if (entryIdx >= desc.entries.size()) break;
            const auto& e = desc.entries[entryIdx++];

            VkWriteDescriptorSet w{};
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = m_set;
            w.dstBinding      = m_shifts.apply(le.type, le.binding);
            w.dstArrayElement = 0;
            w.descriptorCount = 1;
            w.descriptorType  = toVkDescriptorType(le);

            switch (le.type) {
            case BindingType::UniformBuffer: case BindingType::StorageBufferReadOnly: case BindingType::StorageBufferReadWrite:
                if (auto* vkBuf = static_cast<VkBufferImpl*>(e.buffer)) {
                    VkDescriptorBufferInfo bi{}; bi.buffer = vkBuf->handle(); bi.offset = e.bufferOffset;
                    bi.range = e.bufferSize > 0 ? e.bufferSize : VK_WHOLE_SIZE;
                    bufInfos.push_back(bi); w.pBufferInfo = &bufInfos.back();
                } else continue;
                break;
            case BindingType::SampledTexture: case BindingType::StorageTextureReadOnly: case BindingType::StorageTextureReadWrite:
                if (auto* vkView = static_cast<VkTextureViewImpl*>(e.textureView)) {
                    VkDescriptorImageInfo ii{}; ii.imageView = vkView->handle();
                    if (le.type == BindingType::SampledTexture) {
                        // Depth/stencil textures are always sampled in DEPTH_STENCIL_READ_ONLY layout.
                        auto* vkTex = vkView->texture;
                        if (vkTex && isDepthFormat(vkTex->desc.format))
                            ii.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                        else
                            ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    } else {
                        ii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                    }
                    imgInfos.push_back(ii); w.pImageInfo = &imgInfos.back();
                } else continue;
                break;
            case BindingType::Sampler: case BindingType::ComparisonSampler:
                if (auto* vkSamp = static_cast<VkSamplerImpl*>(e.sampler)) {
                    VkDescriptorImageInfo ii{}; ii.sampler = vkSamp->handle();
                    imgInfos.push_back(ii); w.pImageInfo = &imgInfos.back();
                } else continue;
                break;
            case BindingType::AccelerationStructure:
                if (auto* vkAs = static_cast<VkAccelStructImpl*>(e.accelStruct)) {
                    asHandles.push_back(vkAs->handle());
                    VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
                    asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                    asInfo.accelerationStructureCount = 1;
                    asInfo.pAccelerationStructures = &asHandles.back();
                    asWriteInfos.push_back(asInfo);
                    w.pNext = &asWriteInfos.back();
                } else continue;
                break;
            default: continue;
            }
            writes.push_back(w);
        }

        if (!writes.empty())
            vkUpdateDescriptorSets(device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    VkDevice              m_device = VK_NULL_HANDLE;
    VkDescriptorSet       m_set    = VK_NULL_HANDLE;
    VkDescriptorPool      m_pool   = VK_NULL_HANDLE;
    VkBindGroupLayoutImpl* m_layout = nullptr;
    BindingShifts          m_shifts{};
};

} // namespace draco::rhi::vk
