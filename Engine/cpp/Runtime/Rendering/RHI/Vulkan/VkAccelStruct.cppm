/// Vulkan implementation of AccelStruct (acceleration structure).

module;

#include "VkIncludes.h"

export module rhi.vk:accel_struct;

import core.stdtypes;
import core.status;
import rhi;
import :adapter;

using namespace draco;

export namespace draco::rhi::vk {

class VkAccelStructImpl : public AccelStruct {
public:
    Status init(VkDevice device, VkAdapterImpl* adapter, const AccelStructDesc& desc, u64 size) {
        m_type = desc.type;
        m_device = device;

        // Create buffer for the acceleration structure.
        VkBufferCreateInfo bufCi{};
        bufCi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufCi.size  = size;
        bufCi.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        if (vkCreateBuffer(device, &bufCi, nullptr, &m_buffer) != VK_SUCCESS) return ErrorCode::Unknown;

        VkMemoryRequirements memReqs{};
        vkGetBufferMemoryRequirements(device, m_buffer, &memReqs);

        i32 memType = adapter->findMemoryType(static_cast<u32>(memReqs.memoryTypeBits), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType < 0) { vkDestroyBuffer(device, m_buffer, nullptr); m_buffer = VK_NULL_HANDLE; return ErrorCode::Unknown; }

        VkMemoryAllocateFlagsInfo allocFlags{};
        allocFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.pNext = &allocFlags;
        ai.allocationSize  = memReqs.size;
        ai.memoryTypeIndex = static_cast<u32>(memType);
        if (vkAllocateMemory(device, &ai, nullptr, &m_memory) != VK_SUCCESS) {
            vkDestroyBuffer(device, m_buffer, nullptr); m_buffer = VK_NULL_HANDLE; return ErrorCode::Unknown;
        }
        vkBindBufferMemory(device, m_buffer, m_memory, 0);

        // Create acceleration structure.
        VkAccelerationStructureCreateInfoKHR asCi{};
        asCi.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        asCi.buffer = m_buffer;
        asCi.size   = size;
        asCi.type   = desc.type == AccelStructType::TopLevel
            ? VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
            : VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        auto pfnCreate = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
            vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
        if (!pfnCreate || pfnCreate(device, &asCi, nullptr, &m_accel) != VK_SUCCESS) return ErrorCode::Unknown;

        // Get device address.
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addrInfo.accelerationStructure = m_accel;
        auto pfnAddr = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
            vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
        if (pfnAddr) m_deviceAddress = pfnAddr(device, &addrInfo);

        return ErrorCode::Ok;
    }

    void cleanup(VkDevice device) {
        if (m_accel != VK_NULL_HANDLE) {
            auto pfn = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
                vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
            if (pfn) pfn(device, m_accel, nullptr);
            m_accel = VK_NULL_HANDLE;
        }
        if (m_memory != VK_NULL_HANDLE) { vkFreeMemory(device, m_memory, nullptr); m_memory = VK_NULL_HANDLE; }
        if (m_buffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, m_buffer, nullptr); m_buffer = VK_NULL_HANDLE; }
    }

    AccelStructType type()          const override { return m_type; }
    u64             deviceAddress() const override { return m_deviceAddress; }

    [[nodiscard]] VkAccelerationStructureKHR handle() const { return m_accel; }

private:
    VkAccelerationStructureKHR m_accel  = VK_NULL_HANDLE;
    VkBuffer                   m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory             m_memory = VK_NULL_HANDLE;
    VkDevice                   m_device = VK_NULL_HANDLE;
    AccelStructType            m_type   = AccelStructType::BottomLevel;
    u64                        m_deviceAddress = 0;
};

} // namespace draco::rhi::vk
