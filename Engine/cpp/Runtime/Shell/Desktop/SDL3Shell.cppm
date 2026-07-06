// shell.desktop - the desktop shell target (Windows/Linux/macOS), implemented on SDL3:
// one backend covering Wayland, X11, Win32, and Cocoa, plus input and native-handle
// export for RHI surface creation. This primary module re-exports the window and input
// partitions and declares SDL3Shell; the SDL implementation lives in the matching .cpp
// files (SDL3Shell.cpp, SDL3Window.cpp, SDL3Input.cpp).
//
// We own the entry point (SDL_MAIN_HANDLED), so SDL does not hijack main. If SDL video
// init or window creation fails (e.g. no display), the shell degrades: mainWindow() is
// null and isRunning() is false, so a runner exits immediately rather than crashing.

export module shell.desktop;

export import :window;
export import :input;

import core.stdtypes;
import shell;

export namespace draco::shell
{
    class SDL3Shell final : public IShell
    {
    public:
        // Note on the Wayland Vulkan-window quirk: SDL only attaches libdecor
        // client-side decorations to a window backed by a GPU surface, so a plain
        // window comes up bare on GNOME/Mutter. sdl3WindowFlags() flags every
        // window as Vulkan on Linux (skipped under the "dummy" driver) to fix it.
        explicit SDL3Shell(const WindowSettings& settings = {}) noexcept;
        ~SDL3Shell() override;

        SDL3Shell(const SDL3Shell&) = delete;
        SDL3Shell& operator=(const SDL3Shell&) = delete;

        [[nodiscard]] IWindowManager* windowManager() noexcept override { return &m_windows; }
        [[nodiscard]] IWindow* mainWindow() noexcept override { return m_windows.mainWindow(); }
        [[nodiscard]] IInputManager* input() noexcept override { return &m_input; }

        void processEvents() override;

        [[nodiscard]] bool isRunning() const noexcept override
        {
            IWindow* main = m_windows.mainWindow();
            return m_running && main != nullptr && main->isOpen();
        }

        void requestExit() override { m_running = false; }

    private:
        SDL3WindowManager m_windows;
        SDL3InputManager m_input;
        bool m_initialized = false;
        bool m_running = true;
    };
}
