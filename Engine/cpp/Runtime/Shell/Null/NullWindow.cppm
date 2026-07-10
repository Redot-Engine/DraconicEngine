// shell.null:window - a headless but fully functional window manager (create/destroy/
// resize) so multi-window logic is testable without an OS. Declarations only; the
// implementation lives in NullWindow.cpp.
module;

#include <expected>
#include <memory>
#include <span>
#include <vector>

export module shell.null:window;

import core.stdtypes;
import shell;

export namespace draco::shell
{
    class NullWindow final : public IWindow
    {
    public:
        NullWindow(u32 id, const WindowSettings& settings) noexcept;

        [[nodiscard]] u32 id() const noexcept override;
        [[nodiscard]] u32 width() const noexcept override;
        [[nodiscard]] u32 height() const noexcept override;
        [[nodiscard]] NativeWindow native() const noexcept override;  // headless: no handles
        [[nodiscard]] bool isOpen() const noexcept override;
        [[nodiscard]] bool isMinimized() const noexcept override;
        void close() override;

        // --- test/headless controls (no OS to drive these) ---
        void resize(u32 w, u32 h) noexcept;
        void setMinimized(bool m) noexcept;

    private:
        u32 m_id;
        u32 m_width;
        u32 m_height;
        bool m_open = true;
        bool m_minimized = false;
    };

    class NullWindowManager final : public IWindowManager
    {
    public:
        explicit NullWindowManager(const WindowSettings& main);

        [[nodiscard]] std::expected<IWindow*, WindowError> createWindow(const WindowSettings& settings) override;
        void destroyWindow(IWindow* window) override;

        [[nodiscard]] std::span<IWindow* const> windows() const noexcept override;
        [[nodiscard]] IWindow* mainWindow() const noexcept override;
        [[nodiscard]] IWindow* getWindow(u32 id) const noexcept override;
        [[nodiscard]] std::span<const WindowEvent> events() const noexcept override;
        void flushDestroyed() override;

    private:
        // True only for windows this manager owns (present in m_live). Rejects nullptr
        // too, so destroyWindow() is a no-op for null/unknown windows.
        [[nodiscard]] bool owns(IWindow* window) const noexcept;

        std::vector<std::unique_ptr<NullWindow>> m_owned;
        std::vector<IWindow*> m_live;          // borrowed parallel pointers for the span
        std::vector<u32> m_pendingDestroy; // window ids
        std::vector<WindowEvent> m_events;     // always empty (no OS event source)
        u32 m_nextId = 1;
        u32 m_mainWindowId = 0;         // id of the main window (first created); 0 = none
    };
}
