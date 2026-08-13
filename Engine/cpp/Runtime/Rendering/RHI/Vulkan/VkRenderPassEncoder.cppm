/// Vulkan implementation of RenderPassEncoder + MeshShaderPassExt.

module;

#include "VkIncludes.h"
#include <vector>
#include <span>

export module rhi.vk:render_pass_encoder;

import core.stdtypes;
import core.status;
import rhi;
import :conversions;
import :buffer;
import :bind_group;
import :render_pipeline;
import :compute_pipeline;
import :pipeline_layout;
import :query_set;
import :mesh_pipeline;
import :render_bundle_encoder;

using namespace draco;

export namespace draco::rhi::vk {

class VkRenderPassEncoderImpl : public RenderPassEncoder, public MeshShaderPassExt {
public:
    MeshShaderPassExt* asMeshShaderExt() noexcept override { return this; }
    VkRenderPassEncoderImpl(VkCommandBuffer cmdBuf, VkDevice device)
        : m_cmdBuf(cmdBuf), m_device(device) {}

    // ---- RenderPassEncoder ----

    void setPipeline(RenderPipeline* pipeline) override {
        m_currentPipeline = static_cast<VkRenderPipelineImpl*>(pipeline);
        m_currentMeshPipeline = nullptr;
        if (m_currentPipeline)
            vkCmdBindPipeline(m_cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_currentPipeline->handle());
    }

    void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynOffsets) override {
        auto* bg = static_cast<VkBindGroupImpl*>(group);
        auto* layout = getCurrentLayout();
        if (!bg || !layout) return;
        VkDescriptorSet set = bg->handle();
        vkCmdBindDescriptorSets(m_cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout->handle(),
            index, 1, &set, static_cast<u32>(dynOffsets.size()), dynOffsets.data());
    }

    void setPushConstants(ShaderStage stages, u32 offset, u32 size, const void* data) override {
        auto* layout = getCurrentLayout();
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

    void setViewport(f32 x, f32 y, f32 w, f32 h, f32 minDepth, f32 maxDepth) override {
        // Flip Y via negative height to match DX12 coordinate system.
        VkViewport vp{}; vp.x = x; vp.y = y + h; vp.width = w; vp.height = -h;
        vp.minDepth = minDepth; vp.maxDepth = maxDepth;
        vkCmdSetViewport(m_cmdBuf, 0, 1, &vp);
    }

    void setScissor(i32 x, i32 y, u32 w, u32 h) override {
        VkRect2D sc{}; sc.offset = {x, y}; sc.extent = {w, h};
        vkCmdSetScissor(m_cmdBuf, 0, 1, &sc);
    }

    void setBlendConstant(f32 r, f32 g, f32 b, f32 a) override {
        f32 c[4] = {r, g, b, a};
        vkCmdSetBlendConstants(m_cmdBuf, c);
    }

    void setStencilReference(u32 ref) override {
        vkCmdSetStencilReference(m_cmdBuf, VK_STENCIL_FACE_FRONT_AND_BACK, ref);
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

    void writeTimestamp(QuerySet* qs, u32 index) override {
        auto* q = static_cast<VkQuerySetImpl*>(qs);
        if (q) vkCmdWriteTimestamp(m_cmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, q->handle(), index);
    }

    void beginOcclusionQuery(QuerySet* qs, u32 index) override {
        auto* q = static_cast<VkQuerySetImpl*>(qs);
        if (q) vkCmdBeginQuery(m_cmdBuf, q->handle(), index, 0);
    }

    void endOcclusionQuery(QuerySet* qs, u32 index) override {
        auto* q = static_cast<VkQuerySetImpl*>(qs);
        if (q) vkCmdEndQuery(m_cmdBuf, q->handle(), index);
    }

    void executeBundles(std::span<RenderBundle* const> bundles) override {
        if (bundles.empty()) return;
        std::vector<VkCommandBuffer> secs(bundles.size());
        for (usize i = 0; i < bundles.size(); ++i)
            secs[i] = static_cast<VkRenderBundleImpl*>(bundles[i])->handle();
        vkCmdExecuteCommands(m_cmdBuf, static_cast<u32>(secs.size()), secs.data());
    }

    void end() override {
        vkCmdEndRendering(m_cmdBuf);
        m_currentPipeline = nullptr;
        m_currentMeshPipeline = nullptr;
    }

    // ---- MeshShaderPassExt ----

    void setMeshPipeline(MeshPipeline* pipeline) override;
    void drawMeshTasks(u32 gx, u32 gy, u32 gz) override;
    void drawMeshTasksIndirect(Buffer* buf, u64 offset, u32 drawCount, u32 stride) override;
    void drawMeshTasksIndirectCount(Buffer* buf, u64 offset, Buffer* countBuf, u64 countOffset, u32 maxDrawCount, u32 stride) override;

private:
    VkPipelineLayoutImpl* getCurrentLayout();

    VkCommandBuffer       m_cmdBuf = VK_NULL_HANDLE;
    VkDevice              m_device = VK_NULL_HANDLE;
    VkRenderPipelineImpl* m_currentPipeline     = nullptr;
    VkMeshPipelineImpl*   m_currentMeshPipeline = nullptr;

    // Cached device-level mesh shader function pointers.
    PFN_vkCmdDrawMeshTasksEXT              m_pfnDrawMesh          = nullptr;
    PFN_vkCmdDrawMeshTasksIndirectEXT      m_pfnDrawMeshIndirect  = nullptr;
    PFN_vkCmdDrawMeshTasksIndirectCountEXT m_pfnDrawMeshIndCount  = nullptr;
};

} // namespace draco::rhi::vk
