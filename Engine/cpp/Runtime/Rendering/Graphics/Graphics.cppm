// The `graphics` module.
//
// The RHI render host, promoted out of the per-sample bring-up code so samples,
// the UI, and the renderer share one tested path. Two pieces:
//
//   GraphicsDevice — the SHARED GPU: backend (validation-wrapped) + adapter +
//     logical device + graphics queue, plus the CPU frame-in-flight ring index.
//     Created once for the whole app. Hands out RenderWindows.
//
//   RenderWindow — a single window's PRESENTATION target: surface + swapchain +
//     a per-window ring of command pools/fences. Created/destroyed at runtime
//     (the basis for detachable UI windows). The main window is just the first.
//
// A FrameContext is the per-window, per-frame hand-off the host gives a consumer:
// an acquired backbuffer + a host-created encoder. The host owns acquire / fence
// sync / submit / present and the backbuffer state transitions; the consumer
// records content (or calls beginBackbufferPass for the common clear+pass case).
//
// Multi-window is uniform: there is no "main window" special case here — an app
// renders a list of RenderWindows, each independent, sharing one GraphicsDevice.

module;

#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

export module graphics;

import core.stdtypes;
import core.status;
import rhi;
import shell;
// Backend factories live in sibling modules so this core host imports only the
// base RHI (keeps it GPU-backend-agnostic and avoids importing heavy backend
// modules into this interface — which GCC's module reader chokes on):
//   graphics.null — createNullGraphicsDevice (headless)
//   graphics.gpu  — createGraphicsDevice (Vulkan/DX12)

using namespace draco;
using namespace draco::shell;   // IShell + input/window types
namespace rhi = draco::rhi;

export namespace draco::graphics
{
    // Null is a real headless option (CI / servers / tests) — no GPU required.
    enum class BackendType : u8 { Vulkan, DX12, Null };

    struct GraphicsDeviceDesc
    {
        BackendType        backend          = BackendType::Vulkan;
        // Validation (our RHI-layer wrapper AND the backend's own layers, e.g. Vulkan validation)
        // defaults ON for dev builds and OFF for optimized/shipping builds (NDEBUG — set for
        // Release/RelWithDebInfo/MinSizeRel). A profiling run (RelWithDebInfo) therefore measures
        // the real cost, not the validation overhead. Override explicitly to force either way.
#if defined(NDEBUG)
        bool               enableValidation = false;
#else
        bool               enableValidation = true;
#endif
        u32                 framesInFlight    = 2;     // CPU-ahead ring depth
        rhi::DeviceFeatures requiredFeatures = {};
    };

    struct RenderWindowDesc
    {
        rhi::TextureFormat format      = rhi::TextureFormat::BGRA8UnormSrgb;
        rhi::PresentMode   presentMode = rhi::PresentMode::Fifo;
        u32                 bufferCount = 2;            // swapchain images
    };

    class GraphicsDevice;   // forward (RenderWindow refs it; defined complete below)
    class RenderWindow;     // forward (FrameContext refs it)

    // Typed per-window payload. The UI layer stashes its {RootView, VGContext,
    // VGRenderer} here without the host knowing the type — capability as*() idiom.
    class IRenderWindowData
    {
    public:
        virtual ~IRenderWindowData() = default;
    };

    // Per-window, per-frame hand-off. Returned by RenderWindow::beginFrame and
    // passed to consumers (Subsystem::render / IApplicationModule::onRenderWindow).
    struct FrameContext
    {
        bool                 valid          = false;   // false => skip (minimized/acquire failed)
        RenderWindow*        window         = nullptr; // which window this frame targets
        u32                   frameIndex     = 0;       // device ring index (0..framesInFlight-1)
        u32                   width          = 0;
        u32                   height         = 0;
        rhi::CommandEncoder* encoder        = nullptr; // primary, host-created
        rhi::CommandPool*    pool           = nullptr; // this frame's pool (extra encoders)
        rhi::Texture*        backbuffer     = nullptr;
        rhi::TextureView*    backbufferView = nullptr;

        // Convenience for the common 2D/UI case: open a render pass that clears
        // and targets the backbuffer (the Undefined->RenderTarget transition was
        // already done by the host in beginFrame). A RenderGraph-driven renderer
        // ignores this and records on `encoder` directly.
        rhi::RenderPassEncoder* beginBackbufferPass(rhi::ClearColor clear)
        {
            rhi::ColorAttachment ca{};
            ca.view       = backbufferView;
            ca.loadOp     = rhi::LoadOp::Clear;
            ca.storeOp    = rhi::StoreOp::Store;
            ca.clearValue = clear;
            rhi::RenderPassDesc rpd{};
            rpd.colorAttachments.push_back(ca);
            m_pass = (encoder != nullptr) ? encoder->beginRenderPass(rpd) : nullptr;
            return m_pass;
        }
        void endBackbufferPass() { if (m_pass != nullptr) { m_pass->end(); m_pass = nullptr; } }

        // One-call clear of the backbuffer (open a clear pass, close it). For
        // minimal apps that just want a visible, cleared window without touching
        // RHI types.
        void clear(f32 r, f32 g, f32 b, f32 a = 1.0f)
        {
            rhi::ClearColor c; c.r = r; c.g = g; c.b = b; c.a = a;
            beginBackbufferPass(c);
            endBackbufferPass();
        }

    private:
        rhi::RenderPassEncoder* m_pass = nullptr;
    };

    // A window's presentation target. Owns its surface/swapchain and a per-frame
    // ring of command pools + fences (per-window present sync). Destroyed via the
    // GraphicsDevice that made it.
    class RenderWindow
    {
    public:
        // Built by GraphicsDevice::createRenderWindow; takes ownership of the RHI
        // objects. Vectors are sized to framesInFlight.
        RenderWindow(GraphicsDevice& device, IWindow& window,
                     rhi::Surface* surface, rhi::SwapChain* swapChain,
                     std::vector<rhi::CommandPool*>&& pools, std::vector<rhi::Fence*>&& fences) noexcept
            : m_device(&device), m_window(&window), m_surface(surface), m_swapChain(swapChain),
              m_pools(std::move(pools)),
              m_fences(std::move(fences)),
              m_width(window.width()), m_height(window.height())
        {
            m_fenceValues.resize(m_fences.size(), 0ull);
        }

        ~RenderWindow();

        RenderWindow(const RenderWindow&) = delete;
        RenderWindow& operator=(const RenderWindow&) = delete;

        [[nodiscard]] IWindow& window() noexcept { return *m_window; }
        [[nodiscard]] rhi::SwapChain* swap() noexcept { return m_swapChain; }

        // Poll the window size; recreate the swapchain if it changed. Returns true
        // when a resize happened.
        bool syncSize();

        // Acquire this window's backbuffer and open a host-created encoder from the
        // current frame's pool (after the per-window fence guards reuse). The
        // returned FrameContext is invalid when the window is minimized/zero-sized
        // or acquisition fails — the caller skips rendering it this frame.
        FrameContext beginFrame();

        // Transition the backbuffer to Present, submit (signalling the per-window
        // fence), and present. No-op for an invalid frame.
        void endFrame(FrameContext& frame);

        [[nodiscard]] IRenderWindowData* data() noexcept { return m_data.get(); }
        void setData(std::unique_ptr<IRenderWindowData> data) noexcept { m_data = std::move(data); }

    private:
        GraphicsDevice* m_device;        // borrowed
        IWindow*        m_window;        // borrowed
        rhi::Surface*   m_surface;       // owned
        rhi::SwapChain* m_swapChain;     // owned
        std::vector<rhi::CommandPool*> m_pools;   // owned, one per frame-in-flight
        std::vector<rhi::Fence*>       m_fences;  // owned, one per frame-in-flight
        std::vector<u64>                m_fenceValues;
        u32 m_width;
        u32 m_height;
        std::unique_ptr<IRenderWindowData> m_data;  // optional typed payload
    };

    class GraphicsDevice
    {
    public:
        // Build a GraphicsDevice from an already-created backend (takes
        // ownership): enumerate adapters, create the logical device + graphics
        // queue. Backend-agnostic — GPU backends (Vulkan/DX12) are built by the
        // `graphics.gpu` factory, which then calls this. On failure the backend is
        // destroyed.
        static std::expected<std::unique_ptr<GraphicsDevice>, ErrorCode> fromBackend(
            rhi::Backend* backend, u32 framesInFlight, const rhi::DeviceFeatures& features = {})
        {
            if (backend == nullptr) { return std::unexpected(ErrorCode::Unknown); }

            std::span<rhi::Adapter* const> adapters = backend->enumerateAdapters();
            if (adapters.size() == 0) { backend->destroy(); return std::unexpected(ErrorCode::Unknown); }

            rhi::DeviceDesc dd{};
            dd.graphicsQueueCount = 1;
            dd.requiredFeatures   = features;
            rhi::Device* device = nullptr;
            if (!adapters[0]->createDevice(dd, device).isOk()) { backend->destroy(); return std::unexpected(ErrorCode::Unknown); }

            rhi::Queue* queue = device->getQueue(rhi::QueueType::Graphics);
            if (queue == nullptr) { device->destroy(); backend->destroy(); return std::unexpected(ErrorCode::Unknown); }

            const u32 frames = framesInFlight == 0 ? 1 : framesInFlight;
            return std::make_unique<GraphicsDevice>(backend, device, queue, frames);
        }

        GraphicsDevice(rhi::Backend* backend, rhi::Device* device, rhi::Queue* queue, u32 framesInFlight) noexcept
            : m_backend(backend), m_device(device), m_queue(queue), m_framesInFlight(framesInFlight) {}

        ~GraphicsDevice()
        {
            if (m_device != nullptr) { m_device->waitIdle(); m_device->destroy(); m_device = nullptr; }
            if (m_backend != nullptr) { m_backend->destroy(); m_backend = nullptr; }
        }

        GraphicsDevice(const GraphicsDevice&) = delete;
        GraphicsDevice& operator=(const GraphicsDevice&) = delete;

        // Create a presentation target (surface + swapchain + per-frame pools/
        // fences) for a window. The window must outlive the RenderWindow.
        std::expected<std::unique_ptr<RenderWindow>, ErrorCode> createRenderWindow(IWindow& window, const RenderWindowDesc& desc)
        {
            const NativeWindow nw = window.native();
            rhi::Surface* surface = nullptr;
            if (!m_backend->createSurface(nw.window, nw.display, surface).isOk()) { return std::unexpected(ErrorCode::Unknown); }

            rhi::SwapChainDesc sd{};
            sd.width       = window.width();
            sd.height      = window.height();
            sd.format      = desc.format;
            sd.presentMode = desc.presentMode;
            sd.bufferCount = desc.bufferCount;
            sd.label       = u8"renderwindow";
            rhi::SwapChain* swapChain = nullptr;
            if (!m_device->createSwapChain(surface, sd, swapChain).isOk())
            {
                m_device->destroySurface(surface);
                return std::unexpected(ErrorCode::Unknown);
            }

            std::vector<rhi::CommandPool*> pools;
            std::vector<rhi::Fence*> fences;
            for (u32 i = 0; i < m_framesInFlight; ++i)
            {
                rhi::CommandPool* pool = nullptr;
                rhi::Fence* fence = nullptr;
                if (!m_device->createCommandPool(rhi::QueueType::Graphics, pool).isOk() ||
                    !m_device->createFence(0, fence).isOk())
                {
                    for (rhi::CommandPool* p : pools) { m_device->destroyCommandPool(p); }
                    for (rhi::Fence* f : fences) { m_device->destroyFence(f); }
                    if (pool != nullptr) { m_device->destroyCommandPool(pool); }
                    m_device->destroySwapChain(swapChain);
                    m_device->destroySurface(surface);
                    return std::unexpected(ErrorCode::Unknown);
                }
                pools.push_back(pool);
                fences.push_back(fence);
            }

            return std::make_unique<RenderWindow>(*this, window, surface, swapChain,
                std::move(pools), std::move(fences));
        }

        [[nodiscard]] rhi::Device* raw() noexcept { return m_device; }
        [[nodiscard]] rhi::Queue*  gfxQueue() noexcept { return m_queue; }
        [[nodiscard]] u32 framesInFlight() const noexcept { return m_framesInFlight; }
        [[nodiscard]] u32 currentFrame() const noexcept { return m_currentFrame; }

        // Advance the CPU frame ring once per app frame (after all windows are
        // rendered). Consumers key per-frame GPU resources on currentFrame().
        void advanceFrame() noexcept { m_currentFrame = (m_currentFrame + 1) % m_framesInFlight; }

    private:
        rhi::Backend* m_backend;   // owned
        rhi::Device*  m_device;    // owned
        rhi::Queue*   m_queue;     // borrowed from device
        u32 m_framesInFlight;
        u32 m_currentFrame = 0;
    };

    // ----- RenderWindow out-of-line defs (need GraphicsDevice complete) -----

    inline RenderWindow::~RenderWindow()
    {
        rhi::Device* dev = m_device->raw();
        if (dev != nullptr) { dev->waitIdle(); }
        for (rhi::CommandPool* p : m_pools) { if (p != nullptr) { dev->destroyCommandPool(p); } }
        for (rhi::Fence* f : m_fences) { if (f != nullptr) { dev->destroyFence(f); } }
        if (m_swapChain != nullptr) { dev->destroySwapChain(m_swapChain); m_swapChain = nullptr; }
        if (m_surface != nullptr) { dev->destroySurface(m_surface); m_surface = nullptr; }
    }

    inline bool RenderWindow::syncSize()
    {
        const u32 nw = m_window->width();
        const u32 nh = m_window->height();
        if (nw == 0 || nh == 0) { return false; }
        if (nw == m_width && nh == m_height) { return false; }
        m_width = nw;
        m_height = nh;
        m_device->raw()->waitIdle();
        m_swapChain->resize(nw, nh);
        return true;
    }

    inline FrameContext RenderWindow::beginFrame()
    {
        if (m_window->isMinimized() || m_window->width() == 0 || m_window->height() == 0) { return FrameContext{}; }

        const u32 fi = m_device->currentFrame();
        // Guard reuse of this slot's pool/backbuffer: wait the GPU's last
        // submission against this window's fence at this ring slot.
        if (m_fenceValues[fi] > 0) { m_fences[fi]->wait(m_fenceValues[fi], ~0ull); }

        if (!m_swapChain->acquireNextImage().isOk()) { return FrameContext{}; }

        m_pools[fi]->reset();
        rhi::CommandEncoder* enc = nullptr;
        if (!m_pools[fi]->createEncoder(enc).isOk() || enc == nullptr) { return FrameContext{}; }

        // Host-managed backbuffer transition (Undefined -> RenderTarget).
        enc->transitionTexture(m_swapChain->currentTexture(),
                               rhi::ResourceState::Undefined, rhi::ResourceState::RenderTarget);

        FrameContext f;
        f.valid          = true;
        f.window         = this;
        f.frameIndex     = fi;
        f.width          = m_window->width();
        f.height         = m_window->height();
        f.encoder        = enc;
        f.pool           = m_pools[fi];
        f.backbuffer     = m_swapChain->currentTexture();
        f.backbufferView = m_swapChain->currentTextureView();
        return f;
    }

    inline void RenderWindow::endFrame(FrameContext& frame)
    {
        if (!frame.valid) { return; }
        const u32 fi = frame.frameIndex;

        frame.encoder->transitionTexture(m_swapChain->currentTexture(),
                                         rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
        rhi::CommandBuffer* cb = frame.encoder->finish();

        ++m_fenceValues[fi];
        rhi::CommandBuffer* cbs[1] = { cb };
        m_device->gfxQueue()->submit(std::span<rhi::CommandBuffer* const>(cbs, 1), m_fences[fi], m_fenceValues[fi]);

        m_swapChain->present(m_device->gfxQueue());
        m_pools[fi]->destroyEncoder(frame.encoder);
        frame.encoder = nullptr;
    }
}
