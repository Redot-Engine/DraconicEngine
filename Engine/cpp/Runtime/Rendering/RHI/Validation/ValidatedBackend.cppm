/// Validation wrapper for Backend.

module;

#include <span>
#include <vector>

export module rhi.validation:validated_backend;

import core.stdtypes;
import core.status;
import rhi;
import :validated_adapter;

using namespace draco;

export namespace draco::rhi::validation {

class ValidatedBackend : public Backend {
public:
    explicit ValidatedBackend(Backend* inner) : m_inner(inner) {
        isInitialized = inner->isInitialized;
    }

    std::span<Adapter* const> enumerateAdapters() override {
        if (!m_inner->isInitialized) {
            logError("[Validation] enumerateAdapters: backend not initialized");
            return {};
        }

        if (m_adapterWrappers.empty()) {
            auto innerAdapters = m_inner->enumerateAdapters();
            m_adapterWrappers.reserve(innerAdapters.size());
            m_adapterPtrs.reserve(innerAdapters.size());
            for (usize i = 0; i < innerAdapters.size(); ++i) {
                auto* w = createValidatedAdapter(innerAdapters[i]);
                m_adapterWrappers.push_back(w);
                m_adapterPtrs.push_back(w);
            }
        }
        return std::span<Adapter* const>(m_adapterPtrs.data(), m_adapterPtrs.size());
    }

    Status createSurface(void* windowHandle, void* displayHandle, Surface*& out) override {
        if (!windowHandle) {
            logError("[Validation] createSurface: windowHandle is null");
            out = nullptr;
            return ErrorCode::InvalidArgument;
        }
        return m_inner->createSurface(windowHandle, displayHandle, out);
    }

    void destroy() override {
        for (auto* w : m_adapterWrappers) delete w;
        m_adapterWrappers.clear();
        m_adapterPtrs.clear();
        m_inner->destroy();
        delete this;
    }

    Backend* inner() const { return m_inner; }

private:
    static ValidatedAdapter* createValidatedAdapter(Adapter* inner);

    Backend* m_inner;
    std::vector<ValidatedAdapter*> m_adapterWrappers;
    std::vector<Adapter*>          m_adapterPtrs;
};

Backend* createValidatedBackend(Backend* inner) {
    if (!inner) { logError("[Validation] createValidatedBackend: inner is null"); return nullptr; }
    return new ValidatedBackend(inner);
}

} // namespace draco::rhi::validation
