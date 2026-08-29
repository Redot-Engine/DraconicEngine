/// Validation wrapper for Fence.

export module rhi.validation:validated_fence;

import core.stdtypes;
import rhi;

using namespace draco;

export namespace draco::rhi::validation {

class ValidatedFence : public Fence {
public:
    explicit ValidatedFence(Fence* inner) : m_inner(inner) {}

    u64 completedValue() override { return m_inner->completedValue(); }

    bool wait(u64 value, u64 timeoutNs) override {
        if (value > m_lastSignaled && m_lastSignaled > 0) {
            logWarningf("[Validation] Fence::wait: waiting for value %llu but highest signaled is %llu",
                        static_cast<unsigned long long>(value), static_cast<unsigned long long>(m_lastSignaled));
        }
        return m_inner->wait(value, timeoutNs);
    }

    void trackSignal(u64 value) {
        if (value <= m_lastSignaled && m_lastSignaled > 0) {
            logWarningf("[Validation] Fence signal value %llu is not monotonically increasing (last=%llu)",
                        static_cast<unsigned long long>(value), static_cast<unsigned long long>(m_lastSignaled));
        }
        m_lastSignaled = value;
    }

    Fence* inner() const { return m_inner; }

private:
    Fence* m_inner;
    u64    m_lastSignaled = 0;
};

} // namespace draco::rhi::validation
