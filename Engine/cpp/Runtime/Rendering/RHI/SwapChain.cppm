/// Abstract swap chain for double/triple-buffered presentation.

export module rhi:swapchain;

import core.stdtypes;
import core.status;
import :enums;
import :texture_format;
import :resources;
import :queue;

using namespace draco;

export namespace draco::rhi {

class SwapChain {
public:
    virtual ~SwapChain() = default;

    /// Format of the swap chain back buffers.
    [[nodiscard]] virtual TextureFormat format() const = 0;
    /// Width in pixels.
    [[nodiscard]] virtual u32 width()  const = 0;
    /// Height in pixels.
    [[nodiscard]] virtual u32 height() const = 0;
    /// Number of back buffers.
    [[nodiscard]] virtual u32 bufferCount() const = 0;
    /// Index of the currently acquired back buffer.
    [[nodiscard]] virtual u32 currentImageIndex() const = 0;

    /// Acquire the next back buffer for rendering. Must be called
    /// before accessing currentTexture / currentTextureView.
    virtual Status acquireNextImage() = 0;

    /// The texture of the currently acquired back buffer.
    [[nodiscard]] virtual Texture*     currentTexture()     = 0;
    /// A view of the currently acquired back buffer.
    [[nodiscard]] virtual TextureView* currentTextureView() = 0;

    /// Present the current back buffer to the screen.
    virtual Status present(Queue* queue) = 0;
    /// Resize the swap chain (e.g. after a window resize).
    virtual Status resize(u32 width, u32 height) = 0;
};

} // namespace draco::rhi
