/// Validation wrapper for CommandEncoder + RayTracingEncoderExt.

module;

#include <span>
#include <string_view>
#include <vector>

export module rhi.validation:validated_command_encoder;

import core.stdtypes;
import rhi;
import :validated_render_pass_encoder;
import :validated_compute_pass_encoder;
import :validated_render_bundle_encoder;

using namespace draco;

export namespace draco::rhi::validation {

enum class EncoderState { Recording, InRenderPass, InComputePass, Finished };

class ValidatedCommandEncoder : public CommandEncoder, public RayTracingEncoderExt {
public:
    RayTracingEncoderExt* asRayTracingExt() noexcept override { return this; }
    explicit ValidatedCommandEncoder(CommandEncoder* inner)
        : m_inner(inner) {}

    ~ValidatedCommandEncoder() override {
        // Bundle encoders (and the bundles they own) live until the command encoder is
        // destroyed - by then its submission has completed, so the inner bundles are done.
        for (auto* e : m_bundleEncoders) delete e;
    }

    // Called by sub-encoders when their end() fires.
    void onPassEnded() { m_state = EncoderState::Recording; }

    // ---- CommandEncoder ----

    RenderPassEncoder* beginRenderPass(const RenderPassDesc& desc) override {
        if (!checkState("beginRenderPass", EncoderState::Recording)) return &m_rpe;
        if (desc.colorAttachments.empty() &&
            (!desc.depthStencilAttachment.has_value() || !desc.depthStencilAttachment->view))
            logWarning("[Validation] beginRenderPass: no color or depth attachment");
        for (usize i = 0; i < desc.colorAttachments.size(); ++i)
            if (!desc.colorAttachments[i].view)
                logErrorf("[Validation] beginRenderPass: color attachment %d view is null", static_cast<int>(i));

        m_state = EncoderState::InRenderPass;
        auto* innerRpe = m_inner->beginRenderPass(desc);
        m_rpe.begin(innerRpe, this);
        return &m_rpe;
    }

    ComputePassEncoder* beginComputePass(std::u8string_view label) override {
        if (!checkState("beginComputePass", EncoderState::Recording)) return &m_cpe;
        m_state = EncoderState::InComputePass;
        auto* innerCpe = m_inner->beginComputePass(label);
        m_cpe.begin(innerCpe, this);
        return &m_cpe;
    }

    RenderBundleEncoder* createRenderBundleEncoder(const RenderBundleDesc& desc) override {
        if (!checkState("createRenderBundleEncoder", EncoderState::Recording)) return nullptr;
        auto* inner = m_inner->createRenderBundleEncoder(desc);
        if (!inner) return nullptr;   // backend does not support bundles
        auto* wrapped = new ValidatedRenderBundleEncoder(inner);
        m_bundleEncoders.push_back(wrapped);   // owned: freed in this encoder's destructor
        return wrapped;
    }

    void barrier(const BarrierGroup& group) override {
        if (!checkState("barrier", EncoderState::Recording)) return;
        m_inner->barrier(group);
    }

    void copyBufferToBuffer(Buffer* src, u64 srcOff, Buffer* dst, u64 dstOff, u64 size) override {
        if (!checkState("copyBufferToBuffer", EncoderState::Recording)) return;
        if (!src) { logError("[Validation] copyBufferToBuffer: src is null"); return; }
        if (!dst) { logError("[Validation] copyBufferToBuffer: dst is null"); return; }
        if (size == 0) logWarning("[Validation] copyBufferToBuffer: size is 0");
        m_inner->copyBufferToBuffer(src, srcOff, dst, dstOff, size);
    }

    void copyBufferToTexture(Buffer* src, Texture* dst, const BufferTextureCopyRegion& r) override {
        if (!checkState("copyBufferToTexture", EncoderState::Recording)) return;
        if (!src) { logError("[Validation] copyBufferToTexture: src is null"); return; }
        if (!dst) { logError("[Validation] copyBufferToTexture: dst is null"); return; }
        m_inner->copyBufferToTexture(src, dst, r);
    }

    void copyTextureToBuffer(Texture* src, Buffer* dst, const BufferTextureCopyRegion& r) override {
        if (!checkState("copyTextureToBuffer", EncoderState::Recording)) return;
        if (!src) { logError("[Validation] copyTextureToBuffer: src is null"); return; }
        if (!dst) { logError("[Validation] copyTextureToBuffer: dst is null"); return; }
        m_inner->copyTextureToBuffer(src, dst, r);
    }

    void copyTextureToTexture(Texture* src, Texture* dst, const TextureCopyRegion& r) override {
        if (!checkState("copyTextureToTexture", EncoderState::Recording)) return;
        if (!src) { logError("[Validation] copyTextureToTexture: src is null"); return; }
        if (!dst) { logError("[Validation] copyTextureToTexture: dst is null"); return; }
        m_inner->copyTextureToTexture(src, dst, r);
    }

    void blit(Texture* src, Texture* dst) override {
        if (!checkState("blit", EncoderState::Recording)) return;
        if (!src || !dst) { logError("[Validation] blit: src or dst is null"); return; }
        m_inner->blit(src, dst);
    }

    void generateMipmaps(Texture* tex) override {
        if (!checkState("generateMipmaps", EncoderState::Recording)) return;
        if (!tex) { logError("[Validation] generateMipmaps: texture is null"); return; }
        m_inner->generateMipmaps(tex);
    }

    void resolveTexture(Texture* src, Texture* dst) override {
        if (!checkState("resolveTexture", EncoderState::Recording)) return;
        if (!src || !dst) { logError("[Validation] resolveTexture: src or dst is null"); return; }
        m_inner->resolveTexture(src, dst);
    }

    void resetQuerySet(QuerySet* qs, u32 first, u32 count) override {
        if (!checkState("resetQuerySet", EncoderState::Recording)) return;
        if (!qs) { logError("[Validation] resetQuerySet: querySet is null"); return; }
        m_inner->resetQuerySet(qs, first, count);
    }

    void writeTimestamp(QuerySet* qs, u32 index) override {
        if (!checkState("writeTimestamp", EncoderState::Recording)) return;
        if (!qs) { logError("[Validation] writeTimestamp: querySet is null"); return; }
        m_inner->writeTimestamp(qs, index);
    }

    void resolveQuerySet(QuerySet* qs, u32 first, u32 count, Buffer* dst, u64 dstOff) override {
        if (!checkState("resolveQuerySet", EncoderState::Recording)) return;
        if (!qs) { logError("[Validation] resolveQuerySet: querySet is null"); return; }
        if (!dst) { logError("[Validation] resolveQuerySet: dst is null"); return; }
        m_inner->resolveQuerySet(qs, first, count, dst, dstOff);
    }

    void beginDebugLabel(std::u8string_view label, f32 r, f32 g, f32 b, f32 a) override {
        if (m_state == EncoderState::Finished) { logError("[Validation] beginDebugLabel: encoder finished"); return; }
        m_debugLabelDepth++;
        m_inner->beginDebugLabel(label, r, g, b, a);
    }

    void endDebugLabel() override {
        if (m_state == EncoderState::Finished) { logError("[Validation] endDebugLabel: encoder finished"); return; }
        if (m_debugLabelDepth <= 0) { logError("[Validation] endDebugLabel: no matching begin"); return; }
        m_debugLabelDepth--;
        m_inner->endDebugLabel();
    }

    void insertDebugLabel(std::u8string_view label, f32 r, f32 g, f32 b, f32 a) override {
        if (m_state == EncoderState::Finished) return;
        m_inner->insertDebugLabel(label, r, g, b, a);
    }

    CommandBuffer* finish() override {
        if (m_state == EncoderState::Finished) { logError("[Validation] finish: encoder already finished"); return nullptr; }
        if (m_state == EncoderState::InRenderPass) logError("[Validation] finish: render pass still open");
        if (m_state == EncoderState::InComputePass) logError("[Validation] finish: compute pass still open");
        if (m_debugLabelDepth > 0) logWarningf("[Validation] finish: %d debug label(s) not closed", m_debugLabelDepth);
        m_state = EncoderState::Finished;
        return m_inner->finish();
    }

    // ---- RayTracingEncoderExt ----

    void buildBottomLevelAccelStruct(AccelStruct* dst, Buffer* scratch, u64 scratchOff,
                                     std::span<const AccelStructGeometryTriangles> tris,
                                     std::span<const AccelStructGeometryAABBs> aabbs) override {
        if (!checkState("buildBLAS", EncoderState::Recording)) return;
        if (!dst) { logError("[Validation] buildBLAS: dst is null"); return; }
        if (!scratch) { logError("[Validation] buildBLAS: scratch is null"); return; }
        auto* rt = m_inner->asRayTracingExt();
        if (rt) rt->buildBottomLevelAccelStruct(dst, scratch, scratchOff, tris, aabbs);
        else logError("[Validation] buildBLAS: inner encoder does not support ray tracing");
    }

    void buildTopLevelAccelStruct(AccelStruct* dst, Buffer* scratch, u64 scratchOff,
                                  Buffer* instanceBuf, u64 instanceOff, u32 instanceCount) override {
        if (!checkState("buildTLAS", EncoderState::Recording)) return;
        if (!dst || !scratch || !instanceBuf) { logError("[Validation] buildTLAS: null argument"); return; }
        auto* rt = m_inner->asRayTracingExt();
        if (rt) rt->buildTopLevelAccelStruct(dst, scratch, scratchOff, instanceBuf, instanceOff, instanceCount);
        else logError("[Validation] buildTLAS: inner encoder does not support ray tracing");
    }

    void setRayTracingPipeline(RayTracingPipeline* pipeline) override {
        if (!checkState("setRayTracingPipeline", EncoderState::Recording)) return;
        if (!pipeline) { logError("[Validation] setRayTracingPipeline: pipeline is null"); return; }
        m_rtPipelineBound = true;
        auto* rt = m_inner->asRayTracingExt();
        if (rt) rt->setRayTracingPipeline(pipeline);
    }

    void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynOffsets) override {
        if (!checkState("RT setBindGroup", EncoderState::Recording)) return;
        if (!group) { logError("[Validation] RT setBindGroup: group is null"); return; }
        if (!m_rtPipelineBound) logWarning("[Validation] RT setBindGroup: no RT pipeline bound");
        auto* rt = m_inner->asRayTracingExt();
        if (rt) rt->setBindGroup(index, group, dynOffsets);
    }

    void setPushConstants(ShaderStage stages, u32 offset, u32 size, const void* data) override {
        if (!checkState("RT setPushConstants", EncoderState::Recording)) return;
        if (!m_rtPipelineBound) logWarning("[Validation] RT setPushConstants: no RT pipeline bound");
        if (!data && size > 0) { logError("[Validation] RT setPushConstants: data is null"); return; }
        auto* rt = m_inner->asRayTracingExt();
        if (rt) rt->setPushConstants(stages, offset, size, data);
    }

    void traceRays(Buffer* raygenSBT, u64 raygenOff, u64 raygenStride,
                   Buffer* missSBT, u64 missOff, u64 missStride,
                   Buffer* hitSBT, u64 hitOff, u64 hitStride,
                   u32 width, u32 height, u32 depth) override {
        if (!checkState("traceRays", EncoderState::Recording)) return;
        if (!m_rtPipelineBound) { logError("[Validation] traceRays: no RT pipeline bound"); return; }
        if (!raygenSBT) { logError("[Validation] traceRays: raygenSBT is null"); return; }
        auto* rt = m_inner->asRayTracingExt();
        if (rt) rt->traceRays(raygenSBT, raygenOff, raygenStride, missSBT, missOff, missStride,
                              hitSBT, hitOff, hitStride, width, height, depth);
    }

    CommandEncoder* inner() const { return m_inner; }

private:
    bool checkState(const char* method, EncoderState expected) {
        if (m_state == EncoderState::Finished) {
            logErrorf("[Validation] %s: encoder already finished", method);
            return false;
        }
        if (m_state != expected) {
            logErrorf("[Validation] %s: wrong state (expected Recording, got %s)", method,
                      m_state == EncoderState::InRenderPass ? "InRenderPass" :
                      m_state == EncoderState::InComputePass ? "InComputePass" : "?");
            return false;
        }
        return true;
    }

    CommandEncoder* m_inner;
    EncoderState    m_state = EncoderState::Recording;
    i32             m_debugLabelDepth = 0;
    bool            m_rtPipelineBound = false;

    ValidatedRenderPassEncoder  m_rpe;
    ValidatedComputePassEncoder m_cpe;
    std::vector<ValidatedRenderBundleEncoder*> m_bundleEncoders;   // owned wrappers (freed in dtor)
};

// ---- Deferred end() implementations ----

void ValidatedRenderPassEncoder::end() {
    if (m_ended) { logError("[Validation] RenderPassEncoder::end: already ended"); return; }
    m_ended = true;
    m_inner->end();
    if (m_parent) m_parent->onPassEnded();
}

void ValidatedComputePassEncoder::end() {
    if (m_ended) { logError("[Validation] ComputePassEncoder::end: already ended"); return; }
    m_ended = true;
    m_inner->end();
    if (m_parent) m_parent->onPassEnded();
}

} // namespace draco::rhi::validation
