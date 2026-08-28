/// Validation wrapper for SwapChain.

export module rhi.validation:validated_swap_chain;

import core.stdtypes;
import core.status;
import rhi;
import :validated_queue;

using namespace draco;

export namespace draco::rhi::validation {

class ValidatedSwapChain : public SwapChain {
public:
    explicit ValidatedSwapChain(SwapChain* inner) : m_inner(inner) {}

    TextureFormat format()            const override { return m_inner->format(); }
    u32           width()             const override { return m_inner->width(); }
    u32           height()            const override { return m_inner->height(); }
    u32           bufferCount()       const override { return m_inner->bufferCount(); }
    u32           currentImageIndex() const override { return m_inner->currentImageIndex(); }
    Texture*      currentTexture()          override { return m_inner->currentTexture(); }
    TextureView*  currentTextureView()      override { return m_inner->currentTextureView(); }

    Status acquireNextImage() override {
        if (m_imageAcquired) logWarning("[Validation] SwapChain::acquireNextImage: image already acquired");
        Status r = m_inner->acquireNextImage();
        if (r == ErrorCode::Ok) m_imageAcquired = true;
        return r;
    }

    Status present(Queue* queue) override {
        if (!m_imageAcquired) logWarning("[Validation] SwapChain::present: no image acquired");
        m_imageAcquired = false;
        // Unwrap validated queue so the inner swap chain gets the raw queue.
        auto* vq = static_cast<ValidatedQueue*>(queue);
        return m_inner->present(vq ? vq->inner() : queue);
    }

    Status resize(u32 w, u32 h) override {
        if (m_imageAcquired) logError("[Validation] SwapChain::resize: cannot resize while image is acquired");
        if (w == 0 || h == 0) logError("[Validation] SwapChain::resize: dimensions are zero");
        return m_inner->resize(w, h);
    }

    SwapChain* inner() const { return m_inner; }

private:
    SwapChain* m_inner;
    bool       m_imageAcquired = false;
};

} // namespace draco::rhi::validation
