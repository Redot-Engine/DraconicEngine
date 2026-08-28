/// Validation wrapper for ComputePassEncoder.

module;

#include <span>

export module rhi.validation:validated_compute_pass_encoder;

import core.stdtypes;
import rhi;

using namespace draco;

export namespace draco::rhi::validation {

class ValidatedCommandEncoder; // forward

class ValidatedComputePassEncoder : public ComputePassEncoder {
public:
    void begin(ComputePassEncoder* inner, ValidatedCommandEncoder* parent) {
        m_inner = inner; m_parent = parent;
        m_pipelineBound = false; m_ended = false;
    }

    void setPipeline(ComputePipeline* pipeline) override {
        if (m_ended) { logError("[Validation] compute setPipeline: pass ended"); return; }
        if (!pipeline) { logError("[Validation] compute setPipeline: pipeline is null"); return; }
        m_pipelineBound = true;
        m_inner->setPipeline(pipeline);
    }

    void setBindGroup(u32 index, BindGroup* group, std::span<const u32> dynOffsets) override {
        if (m_ended) return;
        if (!group) { logError("[Validation] compute setBindGroup: group is null"); return; }
        if (!m_pipelineBound) logWarning("[Validation] compute setBindGroup: no pipeline bound");
        m_inner->setBindGroup(index, group, dynOffsets);
    }

    void setPushConstants(ShaderStage stages, u32 offset, u32 size, const void* data) override {
        if (m_ended) return;
        if (!m_pipelineBound) logWarning("[Validation] compute setPushConstants: no pipeline bound");
        if (!data && size > 0) { logError("[Validation] compute setPushConstants: data is null"); return; }
        if (offset % 4 != 0) logError("[Validation] compute setPushConstants: offset not 4-byte aligned");
        if (size % 4 != 0) logError("[Validation] compute setPushConstants: size not 4-byte aligned");
        m_inner->setPushConstants(stages, offset, size, data);
    }

    void dispatch(u32 x, u32 y, u32 z) override {
        if (m_ended) { logError("[Validation] dispatch: pass ended"); return; }
        if (!m_pipelineBound) { logError("[Validation] dispatch: no pipeline bound"); return; }
        if (x == 0 || y == 0 || z == 0) logWarning("[Validation] dispatch: zero dimension");
        m_inner->dispatch(x, y, z);
    }

    void dispatchIndirect(Buffer* buffer, u64 offset) override {
        if (m_ended) { logError("[Validation] dispatchIndirect: pass ended"); return; }
        if (!m_pipelineBound) { logError("[Validation] dispatchIndirect: no pipeline bound"); return; }
        if (!buffer) { logError("[Validation] dispatchIndirect: buffer is null"); return; }
        m_inner->dispatchIndirect(buffer, offset);
    }

    void computeBarrier() override {
        if (m_ended) return;
        m_inner->computeBarrier();
    }

    void writeTimestamp(QuerySet* qs, u32 index) override {
        if (m_ended) return;
        if (!qs) { logError("[Validation] compute writeTimestamp: querySet is null"); return; }
        m_inner->writeTimestamp(qs, index);
    }

    void end() override;

private:
    ComputePassEncoder*      m_inner  = nullptr;
    ValidatedCommandEncoder* m_parent = nullptr;
    bool m_pipelineBound = false;
    bool m_ended         = false;
};

} // namespace draco::rhi::validation
