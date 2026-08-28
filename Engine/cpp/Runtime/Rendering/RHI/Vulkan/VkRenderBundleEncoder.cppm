/// Vulkan implementation of RenderBundleEncoder + RenderBundle.
///
/// A render bundle is a SECONDARY VkCommandBuffer recorded with
/// VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT and dynamic-rendering inheritance info
/// (the attachment formats it is compatible with). It is replayed into a primary pass -
/// begun with VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT - via vkCmdExecuteCommands
/// (RenderPassEncoder::ExecuteBundles). Because bundles carry no pass-level dynamic state and
/// Vulkan secondaries don't inherit it, the encoder records a full-target viewport + scissor up
/// front (from the desc extent). The draw-recording methods mirror VkRenderPassEncoderImpl.

module;

#include "VkIncludes.h"
#include <span>

export module rhi.vk:render_bundle_encoder;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :buffer;
import :bind_group;
import :render_pipeline;
import :pipeline_layout;

using namespace draco;

export namespace draco::rhi::vk {

// An immutable, replayable secondary command buffer. Valid until the owning pool resets.
class VkRenderBundleImpl : public RenderBundle {
public:
    explicit VkRenderBundleImpl(VkCommandBuffer cmdBuf) : m_cmdBuf(cmdBuf) {}
    [[nodiscard]] VkCommandBuffer handle() const noexcept { return m_cmdBuf; }
private:
    VkCommandBuffer m_cmdBuf = VK_NULL_HANDLE;
};

// Records draws into a secondary command buffer. The owning command pool allocates/recycles the
// secondary handle and frees this wrapper (+ the produced bundle) on reset.
class VkRenderBundleEncoderImpl : public RenderBundleEncoder {
public:
    explicit VkRenderBundleEncoderImpl(VkCommandBuffer cmdBuf) : m_cmdBuf(cmdBuf) {}
    ~VkRenderBundleEncoderImpl() override { delete m_bundle; }   // frees the produced bundle wrapper

    void setPipeline(RenderPipeline* pipeline) override {
        m_currentPipeline = static_cast<VkRenderPipelineImpl*>(pipeline);
        if (m_currentPipeline)
            vkCmdBindPipeline(m_cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_currentPipeline->handle());
    }

    void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynOffsets) override {
        auto* bg = static_cast<VkBindGroupImpl*>(group);
        auto* layout = m_currentPipeline ? m_currentPipeline->vkLayout() : nullptr;
        if (!bg || !layout) return;
        VkDescriptorSet set = bg->handle();
        vkCmdBindDescriptorSets(m_cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout->handle(),
            index, 1, &set, static_cast<u32>(dynOffsets.size()), dynOffsets.data());
    }

    void setPushConstants(ShaderStage stages, u32 offset, u32 size, const void* data) override {
        auto* layout = m_currentPipeline ? m_currentPipeline->vkLayout() : nullptr;
        if (!layout) return;
        vkCmdPushConstants(m_cmdBuf, layout->handle(), toVkShaderStageFlags(stages), offset, size, data);
    }

    void setVertexBuffer(u32 slot, Buffer* buffer, u64 offset) override {
        auto* vkBuf = static_cast<VkBufferImpl*>(buffer);
        if (!vkBuf) return;
        VkBuffer handle = vkBuf->handle();
        vkCmdBindVertexBuffers(m_cmdBuf, slot, 1, &handle, &offset);
    }

    void setIndexBuffer(Buffer* buffer, IndexFormat format, u64 offset) override {
        auto* vkBuf = static_cast<VkBufferImpl*>(buffer);
        if (!vkBuf) return;
        vkCmdBindIndexBuffer(m_cmdBuf, vkBuf->handle(), offset, toVkIndexType(format));
    }

    void draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override {
        vkCmdDraw(m_cmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void drawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 baseVertex, u32 firstInstance) override {
        vkCmdDrawIndexed(m_cmdBuf, indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
    }

    void drawIndirect(Buffer* buf, u64 offset, u32 drawCount, u32 stride) override {
        auto* vkBuf = static_cast<VkBufferImpl*>(buf);
        if (!vkBuf) return;
        vkCmdDrawIndirect(m_cmdBuf, vkBuf->handle(), offset, drawCount, stride > 0 ? stride : 16);
    }

    void drawIndexedIndirect(Buffer* buf, u64 offset, u32 drawCount, u32 stride) override {
        auto* vkBuf = static_cast<VkBufferImpl*>(buf);
        if (!vkBuf) return;
        vkCmdDrawIndexedIndirect(m_cmdBuf, vkBuf->handle(), offset, drawCount, stride > 0 ? stride : 20);
    }

    // End recording; the produced bundle (owned by the pool) wraps the secondary buffer.
    RenderBundle* finish() override {
        if (m_finished) { return m_bundle; }
        m_finished = true;
        vkEndCommandBuffer(m_cmdBuf);
        m_bundle = new VkRenderBundleImpl(m_cmdBuf);
        return m_bundle;
    }

    [[nodiscard]] VkRenderBundleImpl* producedBundle() const noexcept { return m_bundle; }

private:
    VkCommandBuffer       m_cmdBuf = VK_NULL_HANDLE;
    VkRenderPipelineImpl* m_currentPipeline = nullptr;
    VkRenderBundleImpl*   m_bundle = nullptr;
    bool                  m_finished = false;
};

} // namespace draco::rhi::vk
