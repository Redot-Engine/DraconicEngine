/// Validation wrapper for CommandPool.

export module rhi.validation:validated_command_pool;

import core.stdtypes;
import core.status;
import rhi;
import :validated_command_encoder;

using namespace draco;

export namespace draco::rhi::validation {

class ValidatedCommandPool : public CommandPool {
public:
    explicit ValidatedCommandPool(CommandPool* inner) : m_inner(inner) {}

    Status createEncoder(CommandEncoder*& out) override {
        CommandEncoder* innerEnc = nullptr;
        Status r = m_inner->createEncoder(innerEnc);
        if (r != ErrorCode::Ok || !innerEnc) { out = nullptr; return r; }
        out = new ValidatedCommandEncoder(innerEnc);
        return ErrorCode::Ok;
    }

    void destroyEncoder(CommandEncoder*& encoder) override {
        if (!encoder) return;
        auto* ve = static_cast<ValidatedCommandEncoder*>(encoder);
        if (ve) {
            CommandEncoder* innerEnc = ve->inner();
            m_inner->destroyEncoder(innerEnc);
            delete ve;
        } else {
            m_inner->destroyEncoder(encoder);
        }
        encoder = nullptr;
    }

    void reset() override { m_inner->reset(); }

    CommandPool* inner() const { return m_inner; }

private:
    CommandPool* m_inner;
};

} // namespace draco::rhi::validation
