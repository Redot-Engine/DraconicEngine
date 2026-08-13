// SampleApp - abstract base for RHI samples. Brings up a window (shell), a Vulkan
// backend (validation-wrapped), device, queue, and swap chain; pumps events, tracks
// timing, and calls onRender(). Resize is detected by polling the window size; the
// loop skips rendering while minimized.

module;

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>

export module samples.rhi.framework:sample_app;

import core.stdtypes;
import core.status;
import rhi;
import rhi.vk;
#ifdef DRACO_HAS_DX12
import rhi.dx12;
#endif
import rhi.validation;
import shell;
import shell.desktop;

using namespace draco;

export namespace draco::samples::framework {

enum class BackendType { Vulkan, DX12 };

class SampleApp {
public:
    explicit SampleApp(BackendType backend = BackendType::Vulkan, bool validation = true)
        : m_backendType(backend), m_validationEnabled(validation) {}
    virtual ~SampleApp() = default;

    SampleApp(const SampleApp&)            = delete;
    SampleApp& operator=(const SampleApp&) = delete;

    int run(int argc = 0, char** argv = nullptr);

protected:
    virtual std::u8string_view  title() const { return u8"Draconic Sample"; }
    virtual rhi::DeviceFeatures requiredFeatures() const { return {}; }
    virtual rhi::TextureFormat  swapChainFormat() const { return rhi::TextureFormat::RGBA8UnormSrgb; }
    virtual rhi::PresentMode    presentMode() const { return rhi::PresentMode::Fifo; }
    virtual u32                 bufferCount() const { return 2; }

    virtual Status onInit()     = 0;
    virtual void   onRender()   = 0;
    virtual void   onResize(u32, u32) {}
    virtual void   onShutdown() = 0;

    shell::IShell* m_shell = nullptr;   // owns the shell (created in init(), freed in shutdown())
    shell::IWindow*   m_window   = nullptr;
    rhi::Backend*       m_backend  = nullptr;
    rhi::Device*        m_device   = nullptr;
    rhi::Queue*         m_graphicsQueue = nullptr;
    rhi::Surface*       m_surface  = nullptr;
    rhi::SwapChain*     m_swapChain = nullptr;

    u32 m_width  = 1280;
    u32 m_height = 720;
    bool m_running = false;
    f32  m_deltaTime = 0.0f;
    f32  m_totalTime = 0.0f;

    void exit() { m_running = false; }

private:
    BackendType m_backendType;
    bool        m_validationEnabled;

    Status init();
    void   mainLoop();
    void   shutdown();
    Status createBackend();
    Status createSwapChain();
    void   checkAndResize();
};

inline int SampleApp::run(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dx12") == 0 || std::strcmp(argv[i], "--d3d12") == 0) {
            m_backendType = BackendType::DX12;
        } else if (std::strcmp(argv[i], "--vk") == 0 || std::strcmp(argv[i], "--vulkan") == 0) {
            m_backendType = BackendType::Vulkan;
        } else if (std::strcmp(argv[i], "--novalidation") == 0) {
            m_validationEnabled = false;
        }
    }

    if (!init().isOk()) { shutdown(); return 1; }
    mainLoop();
    shutdown();
    return 0;
}

inline Status SampleApp::init() {
    shell::WindowSettings ws{};
    ws.title  = title();
    ws.width  = m_width;
    ws.height = m_height;

    m_shell = shell::createShell(ws);
    if (m_shell == nullptr || m_shell->mainWindow() == nullptr) {
        rhi::logError("SampleApp: shell/window init failed"); return ErrorCode::Unknown;
    }
    m_window = m_shell->mainWindow();
    m_width  = m_window->width();
    m_height = m_window->height();

    if (!createBackend().isOk()) return ErrorCode::Unknown;

    const shell::NativeWindow nw = m_window->native();
    if (!m_backend->createSurface(nw.window, nw.display, m_surface).isOk()) {
        rhi::logError("SampleApp: createSurface failed"); return ErrorCode::Unknown;
    }

    auto adapters = m_backend->enumerateAdapters();
    if (adapters.size() == 0) { rhi::logError("SampleApp: no adapters"); return ErrorCode::Unknown; }
    rhi::Adapter* adapter = adapters[0];   // best GPU first

    {
        rhi::AdapterInfo ai = adapter->info();
        const char* backendName = (m_backendType == BackendType::DX12) ? "DX12" : "Vulkan";
        const std::u8string name8 = std::u8string(ai.name);
        std::printf("SampleApp: backend=%s adapter=%s\n",
                    backendName, reinterpret_cast<const char*>(name8.c_str()));
    }

    rhi::DeviceDesc dd{};
    dd.graphicsQueueCount = 1;
    dd.requiredFeatures   = requiredFeatures();
    if (!adapter->createDevice(dd, m_device).isOk()) {
        rhi::logError("SampleApp: createDevice failed"); return ErrorCode::Unknown;
    }

    m_graphicsQueue = m_device->getQueue(rhi::QueueType::Graphics);
    if (m_graphicsQueue == nullptr) { rhi::logError("SampleApp: no graphics queue"); return ErrorCode::Unknown; }

    if (!createSwapChain().isOk()) return ErrorCode::Unknown;
    return onInit();
}

inline Status SampleApp::createBackend() {
    rhi::Backend* raw = nullptr;
    switch (m_backendType) {
    case BackendType::Vulkan: {
        rhi::vk::VkBackendDesc desc{};
        desc.enableValidation = m_validationEnabled;
        if (!rhi::vk::createBackend(desc, raw).isOk()) {
            rhi::logError("SampleApp: vk::createBackend failed"); return ErrorCode::Unknown;
        }
        break;
    }
    case BackendType::DX12: {
#ifdef DRACO_HAS_DX12
        rhi::dx12::DxBackendDesc desc{};
        desc.enableValidation = m_validationEnabled;
        if (!rhi::dx12::createDxBackend(desc, raw).isOk()) {
            rhi::logError("SampleApp: dx12::createDxBackend failed"); return ErrorCode::Unknown;
        }
#else
        rhi::logError("SampleApp: DX12 backend not available on this platform");
        return ErrorCode::Unknown;
#endif
        break;
    }
    }
    m_backend = m_validationEnabled ? rhi::validation::createValidatedBackend(raw) : raw;
    return m_backend != nullptr ? Status{} : Status{ ErrorCode::Unknown };
}

inline Status SampleApp::createSwapChain() {
    rhi::SwapChainDesc desc{};
    desc.width       = m_width;
    desc.height      = m_height;
    desc.format      = swapChainFormat();
    desc.presentMode = presentMode();
    desc.bufferCount = bufferCount();
    desc.label       = u8"main";
    if (!m_device->createSwapChain(m_surface, desc, m_swapChain).isOk()) {
        rhi::logError("SampleApp: createSwapChain failed"); return ErrorCode::Unknown;
    }
    return Status{};
}

inline void SampleApp::mainLoop() {
    using clock = std::chrono::steady_clock;
    using dur   = std::chrono::duration<f32>;

    m_running = true;
    auto lastTime = clock::now();

    while (m_running && m_shell->isRunning()) {
        m_shell->processEvents();

        const auto now = clock::now();
        m_deltaTime = dur(now - lastTime).count();
        lastTime   = now;
        m_totalTime += m_deltaTime;

        if (!m_window->isMinimized()) {
            checkAndResize();
            onRender();
        }
    }
}

inline void SampleApp::checkAndResize() {
    const u32 nw = m_window->width();
    const u32 nh = m_window->height();
    if (nw == 0 || nh == 0) return;
    if (nw == m_width && nh == m_height) return;
    m_width = nw; m_height = nh;
    m_device->waitIdle();
    m_swapChain->resize(m_width, m_height);
    onResize(m_width, m_height);
}

inline void SampleApp::shutdown() {
    if (m_device) m_device->waitIdle();
    onShutdown();
    if (m_swapChain) { m_device->destroySwapChain(m_swapChain); m_swapChain = nullptr; }
    if (m_surface)   { m_device->destroySurface(m_surface);     m_surface   = nullptr; }
    if (m_device)    { m_device->destroy();                    m_device    = nullptr; }
    if (m_backend)   { m_backend->destroy();                   m_backend   = nullptr; }
    if (m_shell != nullptr) { shell::destroyShell(m_shell); m_shell = nullptr; }   // destroys the window + shell
}

} // namespace draco::samples::framework
