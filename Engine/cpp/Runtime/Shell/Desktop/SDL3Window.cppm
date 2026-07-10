// shell.desktop:window - the SDL3 window + window-manager backend. Declarations only;
// the implementation (SDL calls) lives in Window.cpp.
module;

#include <expected>
#include <memory>
#include <span>
#include <vector>

#include <SDL3/SDL.h>

export module shell.desktop:window;

import core.stdtypes;
import shell;

export namespace draco::shell
{
    class SDL3Window final : public IWindow
    {
    public:
        explicit SDL3Window(SDL_Window* window) noexcept;
        ~SDL3Window() override;

        SDL3Window(const SDL3Window&) = delete;
        SDL3Window& operator=(const SDL3Window&) = delete;

        [[nodiscard]] u32 id() const noexcept override;
        [[nodiscard]] u32 width() const noexcept override;
        [[nodiscard]] u32 height() const noexcept override;

        // Extract the real native handles from SDL's window properties so RHI can
        // create its own surface (it does not use SDL's Vulkan helpers).
        [[nodiscard]] NativeWindow native() const noexcept override;

        [[nodiscard]] bool isOpen() const noexcept override;
        [[nodiscard]] bool isMinimized() const noexcept override;
        void close() override;

        // The SDL_Window*, exposed as an opaque handle so the interface carries no SDL
        // type. Backend code (and SDL-aware tests) cast it back to SDL_Window*.
        [[nodiscard]] void* handle() const noexcept;
        void onResized(u32 w, u32 h) noexcept;

    private:
        SDL_Window* m_window;
        u32 m_id = 0;
        u32 m_width = 0;
        u32 m_height = 0;
        bool m_open = true;
    };

    // Owns the SDL windows for the run. The main window is the first created.
    // Window destruction is deferred to flushDestroyed() so a window is never
    // freed mid-frame while the GPU may still reference its swapchain.
    class SDL3WindowManager final : public IWindowManager
    {
    public:
        [[nodiscard]] std::expected<IWindow*, WindowError> createWindow(const WindowSettings& settings) override;
        void destroyWindow(IWindow* window) override;

        [[nodiscard]] std::span<IWindow* const> windows() const noexcept override;
        [[nodiscard]] IWindow* mainWindow() const noexcept override;
        [[nodiscard]] IWindow* getWindow(u32 id) const noexcept override;
        [[nodiscard]] std::span<const WindowEvent> events() const noexcept override;
        void flushDestroyed() override;

        // --- event pump wiring (called by SDL3Shell::processEvents) ---
        SDL3Window* find(u32 id) noexcept;
        void clearEvents() noexcept;
        void pushEvent(const WindowEvent& e);

        // Destroy every window immediately (SDL3Window dtors call SDL_DestroyWindow).
        // The shell calls this before SDL_Quit().
        void destroyAllNow();

    private:
        // True only for windows this manager owns (present in m_live). Rejects
        // nullptr too, so destroyWindow() is a no-op for null/unknown windows.
        [[nodiscard]] bool owns(IWindow* window) const noexcept;

        std::vector<std::unique_ptr<SDL3Window>> m_owned;
        std::vector<IWindow*> m_live;
        std::vector<u32> m_pendingDestroy;
        std::vector<WindowEvent> m_events;
        u32 m_mainWindowId = 0;   // id of the main window (first created); 0 = none
    };
}
