/// Validation wrapper for TransferBatch.

module;

#include <span>

export module rhi.validation:validated_transfer_batch;

import core.stdtypes;
import core.status;
import rhi;
import :validated_fence;

using namespace draco;

export namespace draco::rhi::validation {

class ValidatedTransferBatch : public TransferBatch {
public:
    explicit ValidatedTransferBatch(TransferBatch* inner) : m_inner(inner) {}

    void writeBuffer(Buffer* dst, u64 dstOffset, std::span<const u8> data) override {
        if (m_destroyed) { logError("[Validation] TransferBatch::writeBuffer: batch already destroyed"); return; }
        if (!dst) { logError("[Validation] TransferBatch::writeBuffer: dst is null"); return; }
        if (data.size() == 0) { logWarning("[Validation] TransferBatch::writeBuffer: data is empty"); return; }
        m_pendingWrites++;
        m_inner->writeBuffer(dst, dstOffset, data);
    }

    void writeTexture(Texture* dst, std::span<const u8> data, const TextureDataLayout& layout,
                      Extent3D extent, u32 mipLevel, u32 arrayLayer) override {
        if (m_destroyed) { logError("[Validation] TransferBatch::writeTexture: batch already destroyed"); return; }
        if (!dst) { logError("[Validation] TransferBatch::writeTexture: dst is null"); return; }
        if (data.size() == 0) { logWarning("[Validation] TransferBatch::writeTexture: data is empty"); return; }
        if (extent.width == 0 || extent.height == 0) { logError("[Validation] TransferBatch::writeTexture: extent is zero"); return; }
        m_pendingWrites++;
        m_inner->writeTexture(dst, data, layout, extent, mipLevel, arrayLayer);
    }

    Status submit() override {
        if (m_destroyed) { logError("[Validation] TransferBatch::submit: batch already destroyed"); return ErrorCode::Unknown; }
        if (m_pendingWrites == 0) logWarning("[Validation] TransferBatch::submit: no pending writes");
        m_pendingWrites = 0;
        return m_inner->submit();
    }

    Status submitAsync(Fence* fence, u64 signalValue) override {
        if (m_destroyed) { logError("[Validation] TransferBatch::submitAsync: batch already destroyed"); return ErrorCode::Unknown; }
        if (!fence) { logError("[Validation] TransferBatch::submitAsync: fence is null"); return ErrorCode::Unknown; }
        if (m_pendingWrites == 0) logWarning("[Validation] TransferBatch::submitAsync: no pending writes");
        m_pendingWrites = 0;
        auto* vf = static_cast<ValidatedFence*>(fence);
        Fence* innerFence = vf ? vf->inner() : fence;
        if (vf) vf->trackSignal(signalValue);
        return m_inner->submitAsync(innerFence, signalValue);
    }

    void reset() override { m_pendingWrites = 0; m_inner->reset(); }

    void destroy() override {
        if (m_destroyed) { logWarning("[Validation] TransferBatch::destroy: already destroyed"); return; }
        m_destroyed = true;
        m_inner->destroy();
    }

    TransferBatch* inner() const { return m_inner; }

private:
    TransferBatch* m_inner;
    bool m_destroyed     = false;
    i32  m_pendingWrites = 0;
};

} // namespace draco::rhi::validation
