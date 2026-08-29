/// Validation wrapper for Queue.

module;

#include <span>
#include <vector>

export module rhi.validation:validated_queue;

import core.stdtypes;
import core.status;
import rhi;
import :validated_fence;
import :validated_transfer_batch;

using namespace draco;

export namespace draco::rhi::validation {

class ValidatedQueue : public Queue {
public:
    explicit ValidatedQueue(Queue* inner) : m_inner(inner) { queueType = inner->queueType; }

    void submit(std::span<CommandBuffer* const> cmdBufs) override {
        for (usize i = 0; i < cmdBufs.size(); ++i)
            if (!cmdBufs[i]) logErrorf("[Validation] Queue::submit: commandBuffer[%zu] is null", i);
        m_inner->submit(cmdBufs);
    }

    void submit(std::span<CommandBuffer* const> cmdBufs, Fence* signalFence, u64 signalValue) override {
        if (!signalFence) { logError("[Validation] Queue::submit: signalFence is null"); return; }
        auto* vf = static_cast<ValidatedFence*>(signalFence);
        Fence* innerFence = vf ? vf->inner() : signalFence;
        if (vf) vf->trackSignal(signalValue);
        m_inner->submit(cmdBufs, innerFence, signalValue);
    }

    void submit(std::span<CommandBuffer* const> cmdBufs,
                std::span<Fence* const> waitFences, std::span<const u64> waitValues,
                Fence* signalFence, u64 signalValue) override {
        if (waitFences.size() != waitValues.size())
            logError("[Validation] Queue::submit: waitFences and waitValues count mismatch");
        // Unwrap validated fences for both wait and signal.
        std::vector<Fence*> innerWait(waitFences.size());
        for (usize i = 0; i < waitFences.size(); ++i) {
            auto* vw = static_cast<ValidatedFence*>(waitFences[i]);
            innerWait[i] = vw ? vw->inner() : waitFences[i];
        }
        auto* vf = static_cast<ValidatedFence*>(signalFence);
        Fence* innerSignal = vf ? vf->inner() : signalFence;
        if (vf) vf->trackSignal(signalValue);
        m_inner->submit(cmdBufs, std::span<Fence* const>(innerWait.data(), innerWait.size()), waitValues, innerSignal, signalValue);
    }

    void waitIdle() override { m_inner->waitIdle(); }

    Status createTransferBatch(TransferBatch*& out) override {
        TransferBatch* innerBatch = nullptr;
        Status r = m_inner->createTransferBatch(innerBatch);
        if (r != ErrorCode::Ok || !innerBatch) { out = nullptr; return r; }
        out = new ValidatedTransferBatch(innerBatch);
        return ErrorCode::Ok;
    }

    void destroyTransferBatch(TransferBatch*& batch) override {
        if (!batch) return;
        auto* vt = static_cast<ValidatedTransferBatch*>(batch);
        if (vt) {
            TransferBatch* innerBatch = vt->inner();
            m_inner->destroyTransferBatch(innerBatch);
            delete vt;
        } else {
            m_inner->destroyTransferBatch(batch);
        }
        batch = nullptr;
    }

    f32 timestampPeriod() const override { return m_inner->timestampPeriod(); }

    Queue* inner() const { return m_inner; }

private:
    Queue* m_inner;
};

} // namespace draco::rhi::validation
