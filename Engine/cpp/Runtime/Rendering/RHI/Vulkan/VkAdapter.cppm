/// Vulkan implementation of Adapter.
/// Queries physical device properties, features, and queue families.
/// Provides feature detection and queue family selection.

module;

#include "VkIncludes.h"
#include <string_view>
#include <vector>
#include <string>

#include <cstring>

export module rhi.vk:adapter;

import core.stdtypes;
import core.status;
import rhi;

using namespace draco;

export namespace draco::rhi::vk {

class VkDeviceImpl; // forward

class VkAdapterImpl : public Adapter {
public:
    VkAdapterImpl(VkPhysicalDevice physicalDevice, VkInstance instance)
        : m_physicalDevice(physicalDevice), m_instance(instance)
    {
        vkGetPhysicalDeviceProperties(m_physicalDevice, &m_properties);
        vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_features10);
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);

        u32 qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &qfCount, nullptr);
        m_queueFamilies.resize(qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &qfCount, m_queueFamilies.data());

        queryExtensionSupport();
    }

    // ---- Adapter interface ----

    void getInfo(AdapterInfo& out) override {
        out.name = std::u8string(std::u8string_view(reinterpret_cast<const char8_t*>(m_properties.deviceName)));
        out.vendorId = m_properties.vendorID;
        out.deviceId = m_properties.deviceID;

        switch (m_properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   out.type = AdapterType::DiscreteGpu; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: out.type = AdapterType::IntegratedGpu; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            out.type = AdapterType::Cpu; break;
        default:                                     out.type = AdapterType::Unknown; break;
        }

        out.supportedFeatures = buildFeatures();
    }

    Status createDevice(const DeviceDesc& desc, Device*& out) override;

    // ---- Feature building ----

    DeviceFeatures buildFeatures() const {
        DeviceFeatures f{};

        f.bindlessDescriptors       = m_supportsDescriptorIndexing;
        f.timestampQueries          = m_properties.limits.timestampComputeAndGraphics;
        f.multiDrawIndirect         = m_features10.multiDrawIndirect;
        f.depthClamp                = m_features10.depthClamp;
        f.fillModeWireframe         = m_features10.fillModeNonSolid;
        f.textureCompressionBC      = m_features10.textureCompressionBC;
        f.textureCompressionASTC    = m_features10.textureCompressionASTC_LDR;
        f.independentBlend          = m_features10.independentBlend;
        f.multiViewport             = m_features10.multiViewport;
        f.meshShaders               = m_supportsMeshShader;
        f.rayTracing                = m_supportsRayTracing;
        f.pipelineStatisticsQueries = m_features10.pipelineStatisticsQuery;

        // Mesh shader limits.
        if (m_supportsMeshShader) {
            VkPhysicalDeviceMeshShaderPropertiesEXT meshProps{};
            meshProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
            VkPhysicalDeviceProperties2 p2{};
            p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            p2.pNext = &meshProps;
            vkGetPhysicalDeviceProperties2(m_physicalDevice, &p2);
            f.maxMeshOutputVertices   = meshProps.maxMeshOutputVertices;
            f.maxMeshOutputPrimitives = meshProps.maxMeshOutputPrimitives;
            f.maxMeshWorkgroupSize    = meshProps.maxMeshWorkGroupInvocations;
            f.maxTaskWorkgroupSize    = meshProps.maxTaskWorkGroupInvocations;
        }

        // Limits.
        f.maxBindGroups                    = m_properties.limits.maxBoundDescriptorSets;
        f.maxBindingsPerGroup              = m_properties.limits.maxDescriptorSetUniformBuffers;
        f.maxPushConstantSize              = m_properties.limits.maxPushConstantsSize;
        f.maxTextureDimension2D            = m_properties.limits.maxImageDimension2D;
        f.maxTextureArrayLayers            = m_properties.limits.maxImageArrayLayers;
        f.maxComputeWorkgroupSizeX         = m_properties.limits.maxComputeWorkGroupSize[0];
        f.maxComputeWorkgroupSizeY         = m_properties.limits.maxComputeWorkGroupSize[1];
        f.maxComputeWorkgroupSizeZ         = m_properties.limits.maxComputeWorkGroupSize[2];
        f.maxComputeWorkgroupsPerDimension = m_properties.limits.maxComputeWorkGroupCount[0];
        f.maxBufferSize                    = static_cast<u64>(m_properties.limits.maxStorageBufferRange);
        f.minUniformBufferOffsetAlignment  = static_cast<u32>(m_properties.limits.minUniformBufferOffsetAlignment);
        f.minStorageBufferOffsetAlignment  = static_cast<u32>(m_properties.limits.minStorageBufferOffsetAlignment);
        f.timestampPeriodNs                = static_cast<u32>(m_properties.limits.timestampPeriod);

        return f;
    }

    // ---- Queue family selection ----

    /// Finds the best queue family index for the given type.
    /// Prefers dedicated families for Compute and Transfer.
    i32 findQueueFamily(QueueType type) const {
        const auto count = static_cast<i32>(m_queueFamilies.size());
        switch (type) {
        case QueueType::Graphics:
            for (i32 i = 0; i < count; ++i)
                if (m_queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) return i;
            break;
        case QueueType::Compute:
            // Prefer dedicated (no graphics).
            for (i32 i = 0; i < count; ++i) {
                auto f = m_queueFamilies[i].queueFlags;
                if ((f & VK_QUEUE_COMPUTE_BIT) && !(f & VK_QUEUE_GRAPHICS_BIT)) return i;
            }
            for (i32 i = 0; i < count; ++i)
                if (m_queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) return i;
            break;
        case QueueType::Transfer:
            // Prefer dedicated (no graphics or compute).
            for (i32 i = 0; i < count; ++i) {
                auto f = m_queueFamilies[i].queueFlags;
                if ((f & VK_QUEUE_TRANSFER_BIT) && !(f & VK_QUEUE_GRAPHICS_BIT) && !(f & VK_QUEUE_COMPUTE_BIT))
                    return i;
            }
            for (i32 i = 0; i < count; ++i)
                if (m_queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) return i;
            break;
        }
        return -1;
    }

    /// Finds a memory type index matching the filter and property requirements.
    i32 findMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const {
        for (u32 i = 0; i < m_memoryProperties.memoryTypeCount; ++i) {
            if ((typeFilter & (1 << i)) &&
                (m_memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return static_cast<i32>(i);
        }
        return -1;
    }

    /// Maps a MemoryLocation to VkMemoryPropertyFlags.
    static VkMemoryPropertyFlags getMemoryFlags(MemoryLocation loc) {
        switch (loc) {
        case MemoryLocation::GpuOnly:  return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case MemoryLocation::CpuToGpu: return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case MemoryLocation::GpuToCpu: return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        case MemoryLocation::Auto:     return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
        return 0;
    }

    // ---- Internal accessors ----
    [[nodiscard]] VkPhysicalDevice                       physicalDevice()   const { return m_physicalDevice; }
    [[nodiscard]] const VkPhysicalDeviceProperties&      properties()       const { return m_properties; }
    [[nodiscard]] const VkPhysicalDeviceFeatures&        features10()       const { return m_features10; }
    [[nodiscard]] const VkPhysicalDeviceMemoryProperties& memoryProperties() const { return m_memoryProperties; }
    [[nodiscard]] const std::vector<VkQueueFamilyProperties>& queueFamilies() const { return m_queueFamilies; }

    [[nodiscard]] bool supportsDescriptorIndexing() const { return m_supportsDescriptorIndexing; }
    [[nodiscard]] bool supportsMeshShader()         const { return m_supportsMeshShader; }
    [[nodiscard]] bool supportsRayTracing()         const { return m_supportsRayTracing; }

private:
    void queryExtensionSupport() {
        u32 extCount = 0;
        vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, exts.data());

        for (const auto& ext : exts) {
            const char* name = ext.extensionName;
            if (std::strcmp(name, "VK_KHR_dynamic_rendering") == 0)  m_supportsDynamicRendering = true;
            if (std::strcmp(name, "VK_KHR_timeline_semaphore") == 0) m_supportsTimelineSemaphore = true;
            if (std::strcmp(name, "VK_KHR_synchronization2") == 0)   m_supportsSynchronization2 = true;
            if (std::strcmp(name, "VK_EXT_descriptor_indexing") == 0) m_supportsDescriptorIndexing = true;
            if (std::strcmp(name, "VK_EXT_mesh_shader") == 0)        m_supportsMeshShader = true;
            if (std::strcmp(name, "VK_KHR_ray_tracing_pipeline") == 0) m_supportsRayTracing = true;
        }

        // Vulkan 1.3+: these are core.
        u32 major = VK_API_VERSION_MAJOR(m_properties.apiVersion);
        u32 minor = VK_API_VERSION_MINOR(m_properties.apiVersion);
        if (major > 1 || (major == 1 && minor >= 3)) {
            m_supportsDynamicRendering = true;
            m_supportsTimelineSemaphore = true;
            m_supportsSynchronization2 = true;
            m_supportsDescriptorIndexing = true;
        } else if (major == 1 && minor >= 2) {
            m_supportsTimelineSemaphore = true;
            m_supportsDescriptorIndexing = true;
        }
    }

    VkPhysicalDevice                   m_physicalDevice = VK_NULL_HANDLE;
    VkInstance                         m_instance       = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties         m_properties{};
    VkPhysicalDeviceFeatures           m_features10{};
    VkPhysicalDeviceMemoryProperties   m_memoryProperties{};
    std::vector<VkQueueFamilyProperties> m_queueFamilies;

    bool m_supportsDynamicRendering   = false;
    bool m_supportsTimelineSemaphore  = false;
    bool m_supportsSynchronization2   = false;
    bool m_supportsDescriptorIndexing = false;
    bool m_supportsMeshShader         = false;
    bool m_supportsRayTracing         = false;
};

} // namespace draco::rhi::vk
