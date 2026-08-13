/// Validation wrapper for RenderPassEncoder + MeshShaderPassExt.

module;

#include <span>
#include <vector>

export module rhi.validation:validated_render_pass_encoder;

import core.stdtypes;
import rhi;
import :validated_render_bundle_encoder;

using namespace draco;

export namespace draco::rhi::validation {

class ValidatedCommandEncoder; // forward

class ValidatedRenderPassEncoder : public RenderPassEncoder, public MeshShaderPassExt {
public:
    MeshShaderPassExt* asMeshShaderExt() noexcept override { return this; }
    void begin(RenderPassEncoder* inner, ValidatedCommandEncoder* parent) {
        m_inner = inner; m_parent = parent;
        m_pipelineBound = false; m_viewportSet = false; m_scissorSet = false; m_ended = false;
        m_meshPipelineBound = false;
    }

    // ---- RenderPassEncoder ----

    void setPipeline(RenderPipeline* pipeline) override {
        if (m_ended) { logError("[Validation] setPipeline: render pass ended"); return; }
        if (!pipeline) { logError("[Validation] setPipeline: pipeline is null"); return; }
        m_pipelineBound = true; m_meshPipelineBound = false;
        m_inner->setPipeline(pipeline);
    }

    void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynOffsets) override {
        if (m_ended) { logError("[Validation] setBindGroup: render pass ended"); return; }
        if (!group) { logError("[Validation] setBindGroup: group is null"); return; }
        m_inner->setBindGroup(index, group, dynOffsets);
    }

    void setPushConstants(ShaderStage stages, u32 offset, u32 size, const void* data) override {
        if (m_ended) { logError("[Validation] setPushConstants: render pass ended"); return; }
        if (!m_pipelineBound && !m_meshPipelineBound) logWarning("[Validation] setPushConstants: no pipeline bound");
        if (!data && size > 0) { logError("[Validation] setPushConstants: data is null but size > 0"); return; }
        if (size == 0) { logWarning("[Validation] setPushConstants: size is 0"); return; }
        if (offset % 4 != 0) logError("[Validation] setPushConstants: offset must be 4-byte aligned");
        if (size % 4 != 0) logError("[Validation] setPushConstants: size must be 4-byte aligned");
        m_inner->setPushConstants(stages, offset, size, data);
    }

    void setVertexBuffer(u32 slot, Buffer* buffer, u64 offset) override {
        if (m_ended) { logError("[Validation] setVertexBuffer: render pass ended"); return; }
        if (!buffer) { logError("[Validation] setVertexBuffer: buffer is null"); return; }
        m_inner->setVertexBuffer(slot, buffer, offset);
    }

    void setIndexBuffer(Buffer* buffer, IndexFormat format, u64 offset) override {
        if (m_ended) { logError("[Validation] setIndexBuffer: render pass ended"); return; }
        if (!buffer) { logError("[Validation] setIndexBuffer: buffer is null"); return; }
        m_inner->setIndexBuffer(buffer, format, offset);
    }

    void setViewport(f32 x, f32 y, f32 w, f32 h, f32 minD, f32 maxD) override {
        if (m_ended) { logError("[Validation] setViewport: render pass ended"); return; }
        m_viewportSet = true;
        m_inner->setViewport(x, y, w, h, minD, maxD);
    }

    void setScissor(i32 x, i32 y, u32 w, u32 h) override {
        if (m_ended) { logError("[Validation] setScissor: render pass ended"); return; }
        m_scissorSet = true;
        m_inner->setScissor(x, y, w, h);
    }

    void setBlendConstant(f32 r, f32 g, f32 b, f32 a) override {
        if (m_ended) return;
        m_inner->setBlendConstant(r, g, b, a);
    }

    void setStencilReference(u32 ref) override {
        if (m_ended) return;
        m_inner->setStencilReference(ref);
    }

    void draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override {
        if (!checkDrawReady("draw")) return;
        if (vertexCount == 0) logWarning("[Validation] draw: vertexCount is 0");
        m_inner->draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void drawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 baseVertex, u32 firstInstance) override {
        if (!checkDrawReady("drawIndexed")) return;
        if (indexCount == 0) logWarning("[Validation] drawIndexed: indexCount is 0");
        m_inner->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
    }

    void drawIndirect(Buffer* buffer, u64 offset, u32 drawCount, u32 stride) override {
        if (!checkDrawReady("drawIndirect")) return;
        if (!buffer) { logError("[Validation] drawIndirect: buffer is null"); return; }
        m_inner->drawIndirect(buffer, offset, drawCount, stride);
    }

    void drawIndexedIndirect(Buffer* buffer, u64 offset, u32 drawCount, u32 stride) override {
        if (!checkDrawReady("drawIndexedIndirect")) return;
        if (!buffer) { logError("[Validation] drawIndexedIndirect: buffer is null"); return; }
        m_inner->drawIndexedIndirect(buffer, offset, drawCount, stride);
    }

    void executeBundles(std::span<RenderBundle* const> bundles) override {
        if (m_ended) { logError("[Validation] executeBundles: render pass ended"); return; }
        if (!m_viewportSet) logWarning("[Validation] executeBundles: viewport not set - bundles inherit viewport from the parent pass");
        if (!m_scissorSet)  logWarning("[Validation] executeBundles: scissor not set - bundles inherit scissor from the parent pass");
        // Unwrap each ValidatedRenderBundle to its inner bundle before forwarding.
        std::vector<RenderBundle*> inner(bundles.size());
        for (usize i = 0; i < bundles.size(); ++i) {
            auto* vb = static_cast<ValidatedRenderBundle*>(bundles[i]);
            if (!vb) { logErrorf("[Validation] executeBundles: bundle %d is null", static_cast<int>(i)); return; }
            inner[i] = vb->inner();
        }
        m_inner->executeBundles(std::span<RenderBundle* const>{ inner.data(), inner.size() });
    }

    void writeTimestamp(QuerySet* qs, u32 index) override {
        if (m_ended) return;
        if (!qs) { logError("[Validation] writeTimestamp: querySet is null"); return; }
        m_inner->writeTimestamp(qs, index);
    }

    void beginOcclusionQuery(QuerySet* qs, u32 index) override {
        if (m_ended) return;
        if (!qs) { logError("[Validation] beginOcclusionQuery: querySet is null"); return; }
        m_inner->beginOcclusionQuery(qs, index);
    }

    void endOcclusionQuery(QuerySet* qs, u32 index) override {
        if (m_ended) return;
        m_inner->endOcclusionQuery(qs, index);
    }

    void end() override;

    // ---- MeshShaderPassExt ----

    void setMeshPipeline(MeshPipeline* pipeline) override {
        if (m_ended) { logError("[Validation] setMeshPipeline: render pass ended"); return; }
        if (!pipeline) { logError("[Validation] setMeshPipeline: pipeline is null"); return; }
        m_meshPipelineBound = true; m_pipelineBound = false;
        auto* mp = m_inner->asMeshShaderExt();
        if (mp) mp->setMeshPipeline(pipeline);
        else logError("[Validation] setMeshPipeline: inner encoder does not support mesh shaders");
    }

    void drawMeshTasks(u32 gx, u32 gy, u32 gz) override {
        if (!checkDrawReady("drawMeshTasks")) return;
        auto* mp = m_inner->asMeshShaderExt();
        if (mp) mp->drawMeshTasks(gx, gy, gz);
    }

    void drawMeshTasksIndirect(Buffer* buf, u64 offset, u32 drawCount, u32 stride) override {
        if (!checkDrawReady("drawMeshTasksIndirect")) return;
        if (!buf) { logError("[Validation] drawMeshTasksIndirect: buffer is null"); return; }
        auto* mp = m_inner->asMeshShaderExt();
        if (mp) mp->drawMeshTasksIndirect(buf, offset, drawCount, stride);
    }

    void drawMeshTasksIndirectCount(Buffer* buf, u64 offset, Buffer* countBuf, u64 countOffset, u32 maxDrawCount, u32 stride) override {
        if (!checkDrawReady("drawMeshTasksIndirectCount")) return;
        if (!buf || !countBuf) { logError("[Validation] drawMeshTasksIndirectCount: buffer is null"); return; }
        auto* mp = m_inner->asMeshShaderExt();
        if (mp) mp->drawMeshTasksIndirectCount(buf, offset, countBuf, countOffset, maxDrawCount, stride);
    }

private:
    bool checkDrawReady(const char* method) {
        if (m_ended) { logErrorf("[Validation] %s: render pass ended", method); return false; }
        if (!m_pipelineBound && !m_meshPipelineBound) { logErrorf("[Validation] %s: no pipeline bound", method); return false; }
        if (!m_viewportSet) { logErrorf("[Validation] %s: viewport not set", method); return false; }
        if (!m_scissorSet) { logErrorf("[Validation] %s: scissor not set", method); return false; }
        return true;
    }

    RenderPassEncoder*       m_inner  = nullptr;
    ValidatedCommandEncoder* m_parent = nullptr;
    bool m_pipelineBound     = false;
    bool m_meshPipelineBound = false;
    bool m_viewportSet       = false;
    bool m_scissorSet        = false;
    bool m_ended             = false;
};

} // namespace draco::rhi::validation
