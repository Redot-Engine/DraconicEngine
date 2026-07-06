// shell.null - a headless IShell implementation: no real window or OS events. Useful
// for tests, tools, and headless servers, and as the reference for what a real backend
// must provide. This primary module re-exports the window and input partitions and
// declares NullShell; the implementation lives in the matching .cpp files.
//
// processEvents() is a no-op; isRunning() stays true until requestExit() (or the main
// window is closed), so termination relies on the application requesting exit. The
// window manager is fully functional headless (create/destroy/resize).

export module shell.null;

export import :window;
export import :input;

import core.stdtypes;
import shell;

export namespace draco::shell
{
    class NullShell final : public IShell
    {
    public:
        explicit NullShell(const WindowSettings& settings = {}) noexcept;

        [[nodiscard]] IWindowManager* windowManager() noexcept override { return &m_windows; }
        [[nodiscard]] IWindow* mainWindow() noexcept override { return m_windows.mainWindow(); }
        [[nodiscard]] IInputManager* input() noexcept override { return &m_input; }
        void processEvents() override;
        [[nodiscard]] bool isRunning() const noexcept override
        {
            // Running until requestExit() or the main window is closed/destroyed.
            IWindow* main = m_windows.mainWindow();
            return m_running && main != nullptr && main->isOpen();
        }
        void requestExit() override { m_running = false; }

    private:
        NullWindowManager m_windows;
        NullInputManager m_input;
        bool m_running = true;
    };
}
