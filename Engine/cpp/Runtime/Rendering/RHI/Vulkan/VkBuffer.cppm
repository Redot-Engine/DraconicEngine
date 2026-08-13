/// Vulkan implementation of Buffer.

module;

#include "VkIncludes.h"

export module rhi.vk:buffer;

import core.stdtypes;
import core.status;
import rhi;
import :adapter;
import :conversions;

using namespace draco;

export namespace draco::rhi::vk {

class VkDeviceImpl; // forward

class VkBufferImpl : public Buffer {
public:
    Status init(VkDevice device, VkAdapterImpl* adapter, const BufferDesc& d) {
        desc = d;

        VkBufferCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ci.size        = d.size;
        ci.usage       = toVkBufferUsage(d.usage);
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &ci, nullptr, &m_buffer) != VK_SUCCESS) return ErrorCode::Unknown;

        VkMemoryRequirements memReqs{};
        vkGetBufferMemoryRequirements(device, m_buffer, &memReqs);

        auto memFlags = VkAdapterImpl::getMemoryFlags(d.memory);
        i32 memType = adapter->findMemoryType(static_cast<u32>(memReqs.memoryTypeBits), memFlags);

        if (memType < 0 && d.memory == MemoryLocation::Auto)
            memType = adapter->findMemoryType(static_cast<u32>(memReqs.memoryTypeBits),
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (memType < 0) {
            vkDestroyBuffer(device, m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
            return ErrorCode::Unknown;
        }

        bool needsDeviceAddress = hasFlag(d.usage, BufferUsage::AccelStructInput)
                               || hasFlag(d.usage, BufferUsage::ShaderBindingTable)
                               || hasFlag(d.usage, BufferUsage::AccelStructScratch);

        VkMemoryAllocateFlagsInfo allocFlags{};
        allocFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        if (needsDeviceAddress)
            allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        if (needsDeviceAddress) allocInfo.pNext = &allocFlags;
        allocInfo.allocationSize  = memReqs.size;
        allocInfo.memoryTypeIndex = static_cast<u32>(memType);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS) {
            vkDestroyBuffer(device, m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
            return ErrorCode::Unknown;
        }

        vkBindBufferMemory(device, m_buffer, m_memory, 0);

        // Persistently map host-visible buffers.
        if (d.memory == MemoryLocation::CpuToGpu || d.memory == MemoryLocation::GpuToCpu)
            vkMapMemory(device, m_memory, 0, d.size, 0, &m_mappedPtr);

        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device) {
        if (m_mappedPtr) { vkUnmapMemory(device, m_memory); m_mappedPtr = nullptr; }
        if (m_memory != VK_NULL_HANDLE) { vkFreeMemory(device, m_memory, nullptr); m_memory = VK_NULL_HANDLE; }
        if (m_buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, m_buffer, nullptr); m_buffer = VK_NULL_HANDLE; }
    }

    // ---- Buffer interface ----
    void* map()   override { return m_mappedPtr; }
    void  unmap() override { /* persistently mapped */ }

    // ---- Internal ----
    [[nodiscard]] VkBuffer       handle() const { return m_buffer; }
    [[nodiscard]] VkDeviceMemory memory() const { return m_memory; }

private:
    VkBuffer       m_buffer    = VK_NULL_HANDLE;
    VkDeviceMemory m_memory    = VK_NULL_HANDLE;
    void*          m_mappedPtr = nullptr;
};

} // namespace draco::rhi::vk
