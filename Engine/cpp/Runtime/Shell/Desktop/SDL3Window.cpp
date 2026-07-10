// shell.desktop:window implementation - SDL window creation, native-handle export,
// and the deferred-destruction window manager. SDL is confined to this TU.
module;

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

module shell.desktop;

namespace draco::shell
{
    namespace
    {
        // Builds SDL window-creation flags. On Wayland a Vulkan-backed window is
        // needed for client-side decorations (see SDL3Shell ctor note); skipped
        // under the headless "dummy" driver so tests still get a window.
        [[nodiscard]] SDL_WindowFlags sdl3WindowFlags() noexcept
        {
            SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
#if defined(__linux__)
            const char* driver = SDL_GetCurrentVideoDriver();
            if (driver != nullptr && SDL_strcmp(driver, "dummy") != 0) { flags |= SDL_WINDOW_VULKAN; }
#endif
            return flags;
        }
    }

    // ---- SDL3Window ----

    SDL3Window::SDL3Window(SDL_Window* window) noexcept : m_window(window)
    {
        int w = 0, h = 0;
        SDL_GetWindowSize(m_window, &w, &h);
        m_width = static_cast<u32>(w);
        m_height = static_cast<u32>(h);
        m_id = static_cast<u32>(SDL_GetWindowID(m_window));
    }

    SDL3Window::~SDL3Window() { if (m_window != nullptr) { SDL_DestroyWindow(m_window); } }

    u32 SDL3Window::id() const noexcept { return m_id; }
    u32 SDL3Window::width() const noexcept { return m_width; }
    u32 SDL3Window::height() const noexcept { return m_height; }

    NativeWindow SDL3Window::native() const noexcept
    {
        NativeWindow n;
        if (m_window == nullptr) { return n; }
        const SDL_PropertiesID props = SDL_GetWindowProperties(m_window);
#if defined(_WIN32)
        n.system = WindowSystem::Win32;
        n.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
        n.window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__APPLE__)
        n.system = WindowSystem::Cocoa;
        n.window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#else
        const char* driver = SDL_GetCurrentVideoDriver();
        if (driver != nullptr && SDL_strcmp(driver, "wayland") == 0)
        {
            n.system = WindowSystem::Wayland;
            n.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
            n.window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        }
        else if (driver != nullptr && SDL_strcmp(driver, "x11") == 0)
        {
            n.system = WindowSystem::X11;
            n.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
            n.window = reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)));
        }
#endif
        return n;
    }

    bool SDL3Window::isOpen() const noexcept { return m_open; }
    bool SDL3Window::isMinimized() const noexcept
    {
        return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) != 0;
    }
    void SDL3Window::close() { m_open = false; }
    void* SDL3Window::handle() const noexcept { return m_window; }
    void SDL3Window::onResized(u32 w, u32 h) noexcept { m_width = w; m_height = h; }

    // ---- SDL3WindowManager ----

    std::expected<IWindow*, WindowError> SDL3WindowManager::createWindow(const WindowSettings& settings)
    {
        const std::u8string title(settings.title);
        SDL_Window* window = SDL_CreateWindow(
            reinterpret_cast<const char*>(title.c_str()),
            static_cast<int>(settings.width), static_cast<int>(settings.height),
            sdl3WindowFlags());
        if (window == nullptr) { return std::unexpected(WindowError::CreationFailed); }

        auto wrapped = std::make_unique<SDL3Window>(window);
        IWindow* borrowed = wrapped.get();
        m_owned.push_back(std::move(wrapped));
        m_live.push_back(borrowed);
        if (m_mainWindowId == 0) { m_mainWindowId = borrowed->id(); }  // the first window created is the main window
        return borrowed;
    }

    void SDL3WindowManager::destroyWindow(IWindow* window)
    {
        if (!owns(window)) { return; }  // no-op for null or windows this manager does not own
        window->close();
        m_pendingDestroy.push_back(window->id());
    }

    std::span<IWindow* const> SDL3WindowManager::windows() const noexcept
    {
        return std::span<IWindow* const>(m_live.data(), m_live.size());
    }
    IWindow* SDL3WindowManager::mainWindow() const noexcept
    {
        // Tracked by id, so destroying/flushing the main window never promotes
        // another window into its place.
        return getWindow(m_mainWindowId);
    }
    IWindow* SDL3WindowManager::getWindow(u32 id) const noexcept
    {
        for (IWindow* w : m_live) { if (w->id() == id) { return w; } }
        return nullptr;
    }
    std::span<const WindowEvent> SDL3WindowManager::events() const noexcept
    {
        return std::span<const WindowEvent>(m_events.data(), m_events.size());
    }

    void SDL3WindowManager::flushDestroyed()
    {
        for (u32 id : m_pendingDestroy)
        {
            for (usize i = 0; i < m_live.size(); ++i)
            {
                if (m_live[i]->id() == id) { m_live.erase(m_live.begin() + static_cast<std::ptrdiff_t>(i)); break; }
            }
            for (usize i = 0; i < m_owned.size(); ++i)
            {
                if (m_owned[i]->id() == id) { m_owned.erase(m_owned.begin() + static_cast<std::ptrdiff_t>(i)); break; }  // dtor destroys SDL window
            }
        }
        m_pendingDestroy.clear();
    }

    SDL3Window* SDL3WindowManager::find(u32 id) noexcept
    {
        for (std::unique_ptr<SDL3Window>& w : m_owned) { if (w->id() == id) { return w.get(); } }
        return nullptr;
    }
    void SDL3WindowManager::clearEvents() noexcept { m_events.clear(); }
    void SDL3WindowManager::pushEvent(const WindowEvent& e) { m_events.push_back(e); }

    void SDL3WindowManager::destroyAllNow()
    {
        m_live.clear();
        m_owned.clear();
        m_pendingDestroy.clear();
    }

    bool SDL3WindowManager::owns(IWindow* window) const noexcept
    {
        for (IWindow* w : m_live) { if (w == window) { return true; } }
        return false;
    }
}
