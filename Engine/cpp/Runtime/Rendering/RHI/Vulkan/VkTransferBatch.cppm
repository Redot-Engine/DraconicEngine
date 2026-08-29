/// Vulkan implementation of TransferBatch.

module;

#include "VkIncludes.h"
#include <vector>
#include <span>
#include <algorithm>

#include <cstring>

export module rhi.vk:transfer_batch;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :buffer;
import :texture;
import :fence;

using namespace draco;

export namespace draco::rhi::vk {

class VkTransferBatchImpl : public TransferBatch {
public:
    VkTransferBatchImpl(VkDevice device, VkQueue queue, u32 queueFamilyIndex, VkPhysicalDevice physDevice)
        : m_device(device), m_physDevice(physDevice), m_queue(queue), m_queueFamilyIndex(queueFamilyIndex) {}

    void writeBuffer(Buffer* dst, u64 dstOffset, std::span<const u8> data) override {
        u64 needed = m_stagingOffset + data.size();
        if (ensureStagingBuffer(needed) != ErrorCode::Ok) return;
        void* mapped = m_stagingMapped;
        if (!mapped) return;
        std::memcpy(static_cast<u8*>(mapped) + m_stagingOffset, data.data(), data.size());
        m_bufferCopies.push_back({ dst, dstOffset, m_stagingOffset, data.size() });
        m_stagingOffset = (m_stagingOffset + data.size() + 15) & ~static_cast<u64>(15);
    }

    void writeTexture(Texture* dst, std::span<const u8> data,
                      const TextureDataLayout& layout, Extent3D extent,
                      u32 mipLevel, u32 arrayLayer) override {
        u64 needed = m_stagingOffset + data.size();
        if (ensureStagingBuffer(needed) != ErrorCode::Ok) return;
        void* mapped = m_stagingMapped;
        if (!mapped) return;
        std::memcpy(static_cast<u8*>(mapped) + m_stagingOffset, data.data(), data.size());
        m_textureCopies.push_back({ dst, m_stagingOffset, mipLevel, arrayLayer, extent, layout });
        m_stagingOffset = (m_stagingOffset + data.size() + 15) & ~static_cast<u64>(15);
    }

    Status submit() override {
        if (m_bufferCopies.empty() && m_textureCopies.empty()) return ErrorCode::Ok;
        VkCommandBuffer cmdBuf = recordCommands();
        if (cmdBuf == VK_NULL_HANDLE) return ErrorCode::Unknown;

        VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1; si.pCommandBuffers = &cmdBuf;
        if (vkQueueSubmit(m_queue, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) return ErrorCode::Unknown;
        vkQueueWaitIdle(m_queue);
        cleanupCmdPool();
        return ErrorCode::Ok;
    }

    Status submitAsync(Fence* fence, u64 signalValue) override {
        if (m_bufferCopies.empty() && m_textureCopies.empty()) return ErrorCode::Ok;
        VkCommandBuffer cmdBuf = recordCommands();
        if (cmdBuf == VK_NULL_HANDLE) return ErrorCode::Unknown;

        auto* vkFence = static_cast<VkFenceImpl*>(fence);
        if (!vkFence) return ErrorCode::Unknown;

        VkSemaphore sem = vkFence->handle();
        VkTimelineSemaphoreSubmitInfo tsi{}; tsi.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        tsi.signalSemaphoreValueCount = 1; tsi.pSignalSemaphoreValues = &signalValue;

        VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; si.pNext = &tsi;
        si.commandBufferCount = 1; si.pCommandBuffers = &cmdBuf;
        si.signalSemaphoreCount = 1; si.pSignalSemaphores = &sem;

        vkQueueSubmit(m_queue, 1, &si, VK_NULL_HANDLE);
        m_asyncFence = vkFence; m_asyncValue = signalValue;
        return ErrorCode::Ok;
    }

    void reset() override {
        m_bufferCopies.clear(); m_textureCopies.clear();
        m_stagingOffset = 0;
        cleanupCmdPool();
    }

    void destroy() override {
        if (m_asyncFence) { m_asyncFence->wait(m_asyncValue, ~0ull); m_asyncFence = nullptr; }
        if (m_cmdPool != VK_NULL_HANDLE) { vkDestroyCommandPool(m_device, m_cmdPool, nullptr); m_cmdPool = VK_NULL_HANDLE; }
        if (m_stagingMapped) { vkUnmapMemory(m_device, m_stagingMem); m_stagingMapped = nullptr; }
        if (m_stagingMem != VK_NULL_HANDLE) { vkFreeMemory(m_device, m_stagingMem, nullptr); m_stagingMem = VK_NULL_HANDLE; }
        if (m_stagingBuf != VK_NULL_HANDLE) { vkDestroyBuffer(m_device, m_stagingBuf, nullptr); m_stagingBuf = VK_NULL_HANDLE; }
    }

private:
    struct BufCopy { Buffer* dst; u64 dstOffset; u64 stagingOffset; u64 size; };
    struct TexCopy { Texture* dst; u64 stagingOffset; u32 mipLevel; u32 arrayLayer; Extent3D extent; TextureDataLayout layout; };

    Status ensureStagingBuffer(u64 required) {
        if (m_stagingBuf != VK_NULL_HANDLE && m_stagingSize >= required) return ErrorCode::Ok;
        u64 newSize = std::max(required, std::max(m_stagingSize * 2, static_cast<u64>(4 * 1024 * 1024)));

        // Create staging buffer directly.
        VkBufferCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ci.size = newSize; ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkBuffer newBuf = VK_NULL_HANDLE;
        if (vkCreateBuffer(m_device, &ci, nullptr, &newBuf) != VK_SUCCESS) return ErrorCode::Unknown;

        VkMemoryRequirements memReqs{}; vkGetBufferMemoryRequirements(m_device, newBuf, &memReqs);

        // Find host-visible + host-coherent memory type.
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(m_physDevice, &memProps);
        i32 memType = -1;
        for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
            if ((memReqs.memoryTypeBits & (1 << i)) &&
                (memProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
                    == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            { memType = static_cast<i32>(i); break; }
        }
        if (memType < 0) { vkDestroyBuffer(m_device, newBuf, nullptr); return ErrorCode::Unknown; }

        VkMemoryAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = memReqs.size; ai.memoryTypeIndex = static_cast<u32>(memType);
        VkDeviceMemory newMem = VK_NULL_HANDLE;
        if (vkAllocateMemory(m_device, &ai, nullptr, &newMem) != VK_SUCCESS) {
            vkDestroyBuffer(m_device, newBuf, nullptr); return ErrorCode::Unknown;
        }
        vkBindBufferMemory(m_device, newBuf, newMem, 0);

        void* newMapped = nullptr;
        vkMapMemory(m_device, newMem, 0, newSize, 0, &newMapped);

        // Copy existing data.
        if (m_stagingMapped && m_stagingOffset > 0 && newMapped)
            std::memcpy(newMapped, m_stagingMapped, static_cast<usize>(m_stagingOffset));

        // Free old.
        if (m_stagingMapped) vkUnmapMemory(m_device, m_stagingMem);
        if (m_stagingMem != VK_NULL_HANDLE) vkFreeMemory(m_device, m_stagingMem, nullptr);
        if (m_stagingBuf != VK_NULL_HANDLE) vkDestroyBuffer(m_device, m_stagingBuf, nullptr);

        m_stagingBuf = newBuf; m_stagingMem = newMem; m_stagingMapped = newMapped; m_stagingSize = newSize;
        return ErrorCode::Ok;
    }

    VkCommandBuffer recordCommands() {
        if (m_cmdPool == VK_NULL_HANDLE) {
            VkCommandPoolCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; ci.queueFamilyIndex = m_queueFamilyIndex;
            if (vkCreateCommandPool(m_device, &ci, nullptr, &m_cmdPool) != VK_SUCCESS) return VK_NULL_HANDLE;
        }
        VkCommandBufferAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = m_cmdPool; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
        VkCommandBuffer cb = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(m_device, &ai, &cb) != VK_SUCCESS) return VK_NULL_HANDLE;

        VkCommandBufferBeginInfo bi{}; bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &bi);

        for (const auto& c : m_bufferCopies) {
            auto* dst = static_cast<VkBufferImpl*>(c.dst);
            if (!dst) continue;
            VkBufferCopy r{}; r.srcOffset = c.stagingOffset; r.dstOffset = c.dstOffset; r.size = c.size;
            vkCmdCopyBuffer(cb, m_stagingBuf, dst->handle(), 1, &r);
        }

        for (const auto& c : m_textureCopies) {
            auto* dst = static_cast<VkTextureImpl*>(c.dst);
            if (!dst) continue;
            auto aspect = getAspectMask(dst->desc.format);

            VkImageMemoryBarrier pre{}; pre.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            pre.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            pre.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; pre.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            pre.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; pre.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            pre.image = dst->handle();
            pre.subresourceRange = { aspect, c.mipLevel, 1, c.arrayLayer, 1 };
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &pre);

            VkBufferImageCopy r{}; r.bufferOffset = c.stagingOffset;
            r.imageSubresource = { aspect, c.mipLevel, c.arrayLayer, 1 };
            r.imageExtent = { c.extent.width, c.extent.height, c.extent.depth };
            vkCmdCopyBufferToImage(cb, m_stagingBuf, dst->handle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);

            VkImageMemoryBarrier post = pre;
            post.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; post.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            post.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; post.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &post);
        }

        vkEndCommandBuffer(cb);
        return cb;
    }

    void cleanupCmdPool() {
        if (m_cmdPool != VK_NULL_HANDLE) vkResetCommandPool(m_device, m_cmdPool, 0);
    }

    VkDevice         m_device     = VK_NULL_HANDLE;
    VkPhysicalDevice m_physDevice = VK_NULL_HANDLE;
    VkQueue          m_queue      = VK_NULL_HANDLE;
    u32              m_queueFamilyIndex = 0;
    VkCommandPool    m_cmdPool    = VK_NULL_HANDLE;
    VkBuffer         m_stagingBuf = VK_NULL_HANDLE;
    VkDeviceMemory   m_stagingMem = VK_NULL_HANDLE;
    void*            m_stagingMapped = nullptr;
    u64              m_stagingOffset = 0;
    u64              m_stagingSize   = 0;
    std::vector<BufCopy> m_bufferCopies;
    std::vector<TexCopy> m_textureCopies;
    VkFenceImpl*   m_asyncFence = nullptr;
    u64            m_asyncValue  = 0;
};

} // namespace draco::rhi::vk
