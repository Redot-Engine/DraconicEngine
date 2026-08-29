/// Validation wrappers for RenderBundleEncoder + RenderBundle.
/// Mirrors ValidatedRenderPassEncoder's draw-recording checks for the bundle subset.

module;

#include <span>

export module rhi.validation:validated_render_bundle_encoder;

import core.stdtypes;
import rhi;

using namespace draco;

export namespace draco::rhi::validation {

// Wraps an inner bundle so the validation layer can unwrap it at executeBundles time.
class ValidatedRenderBundle : public RenderBundle {
public:
    explicit ValidatedRenderBundle(RenderBundle* inner) : m_inner(inner) {}
    [[nodiscard]] RenderBundle* inner() const noexcept { return m_inner; }
private:
    RenderBundle* m_inner;
};

// Validates the draw-recording subset (pipeline bound before draws, non-null resources) and
// forwards to the inner bundle encoder. Owns the ValidatedRenderBundle it produces at finish.
class ValidatedRenderBundleEncoder : public RenderBundleEncoder {
public:
    explicit ValidatedRenderBundleEncoder(RenderBundleEncoder* inner) : m_inner(inner) {}
    ~ValidatedRenderBundleEncoder() override { delete m_bundle; }

    void setPipeline(RenderPipeline* pipeline) override {
        if (m_finished) { logError("[Validation] bundle setPipeline: bundle already finished"); return; }
        if (!pipeline) { logError("[Validation] bundle setPipeline: pipeline is null"); return; }
        m_pipelineBound = true;
        m_inner->setPipeline(pipeline);
    }

    void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynOffsets) override {
        if (m_finished) { logError("[Validation] bundle setBindGroup: bundle already finished"); return; }
        if (!group) { logError("[Validation] bundle setBindGroup: group is null"); return; }
        m_inner->setBindGroup(index, group, dynOffsets);
    }

    void setPushConstants(ShaderStage stages, u32 offset, u32 size, const void* data) override {
        if (m_finished) { logError("[Validation] bundle setPushConstants: bundle already finished"); return; }
        if (!data && size > 0) { logError("[Validation] bundle setPushConstants: data is null but size > 0"); return; }
        if (offset % 4 != 0) logError("[Validation] bundle setPushConstants: offset must be 4-byte aligned");
        if (size % 4 != 0) logError("[Validation] bundle setPushConstants: size must be 4-byte aligned");
        m_inner->setPushConstants(stages, offset, size, data);
    }

    void setVertexBuffer(u32 slot, Buffer* buffer, u64 offset) override {
        if (m_finished) { logError("[Validation] bundle setVertexBuffer: bundle already finished"); return; }
        if (!buffer) { logError("[Validation] bundle setVertexBuffer: buffer is null"); return; }
        m_inner->setVertexBuffer(slot, buffer, offset);
    }

    void setIndexBuffer(Buffer* buffer, IndexFormat format, u64 offset) override {
        if (m_finished) { logError("[Validation] bundle setIndexBuffer: bundle already finished"); return; }
        if (!buffer) { logError("[Validation] bundle setIndexBuffer: buffer is null"); return; }
        m_inner->setIndexBuffer(buffer, format, offset);
    }

    void draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override {
        if (!checkDrawReady("bundle draw")) return;
        if (vertexCount == 0) logWarning("[Validation] bundle draw: vertexCount is 0");
        m_inner->draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void drawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 baseVertex, u32 firstInstance) override {
        if (!checkDrawReady("bundle drawIndexed")) return;
        if (indexCount == 0) logWarning("[Validation] bundle drawIndexed: indexCount is 0");
        m_inner->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
    }

    void drawIndirect(Buffer* buffer, u64 offset, u32 drawCount, u32 stride) override {
        if (!checkDrawReady("bundle drawIndirect")) return;
        if (!buffer) { logError("[Validation] bundle drawIndirect: buffer is null"); return; }
        m_inner->drawIndirect(buffer, offset, drawCount, stride);
    }

    void drawIndexedIndirect(Buffer* buffer, u64 offset, u32 drawCount, u32 stride) override {
        if (!checkDrawReady("bundle drawIndexedIndirect")) return;
        if (!buffer) { logError("[Validation] bundle drawIndexedIndirect: buffer is null"); return; }
        m_inner->drawIndexedIndirect(buffer, offset, drawCount, stride);
    }

    RenderBundle* finish() override {
        if (m_finished) { logError("[Validation] bundle finish: already finished"); return m_bundle; }
        m_finished = true;
        RenderBundle* innerBundle = m_inner->finish();
        if (innerBundle == nullptr) { logError("[Validation] bundle finish: inner returned null"); return nullptr; }
        m_bundle = new ValidatedRenderBundle(innerBundle);   // freed by this encoder's destructor
        return m_bundle;
    }

private:
    bool checkDrawReady(const char* method) {
        if (m_finished) { logErrorf("[Validation] %s: bundle already finished", method); return false; }
        if (!m_pipelineBound) { logErrorf("[Validation] %s: no pipeline bound", method); return false; }
        return true;
    }

    RenderBundleEncoder*   m_inner;
    ValidatedRenderBundle* m_bundle = nullptr;
    bool                   m_pipelineBound = false;
    bool                   m_finished      = false;
};

} // namespace draco::rhi::validation
