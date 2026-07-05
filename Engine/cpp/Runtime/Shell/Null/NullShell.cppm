// shell.null - a headless IShell implementation: no real window or OS events.
// Useful for tests, tools, and headless servers, and as the reference for what a
// real backend must provide. processEvents() is a no-op; isRunning() stays true
// until requestExit() (or the main window is closed), so the runner relies on the
// application requesting exit to terminate. The window manager is fully functional
// headless (create/destroy/resize) so multi-window logic is testable without an OS.

module;

#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

export module shell.null;

import core.stdtypes;
import shell;

export namespace draconic::shell
{
    class NullWindow final : public IWindow
    {
    public:
        NullWindow(draco::u32 id, const WindowSettings& settings) noexcept
            : m_id(id), m_width(settings.width), m_height(settings.height) {}

        [[nodiscard]] draco::u32 id() const noexcept override { return m_id; }
        [[nodiscard]] draco::u32 width() const noexcept override { return m_width; }
        [[nodiscard]] draco::u32 height() const noexcept override { return m_height; }
        [[nodiscard]] NativeWindow native() const noexcept override { return {}; }  // headless: no handles
        [[nodiscard]] bool isOpen() const noexcept override { return m_open; }
        [[nodiscard]] bool isMinimized() const noexcept override { return m_minimized; }
        void close() override { m_open = false; }

        // --- test/headless controls (no OS to drive these) ---
        void resize(draco::u32 w, draco::u32 h) noexcept { m_width = w; m_height = h; }
        void setMinimized(bool m) noexcept { m_minimized = m; }

    private:
        draco::u32 m_id;
        draco::u32 m_width;
        draco::u32 m_height;
        bool m_open = true;
        bool m_minimized = false;
    };

    class NullWindowManager final : public IWindowManager
    {
    public:
        explicit NullWindowManager(const WindowSettings& main) { (void)createWindow(main); }

        [[nodiscard]] std::expected<IWindow*, WindowError> createWindow(const WindowSettings& settings) override
        {
            const draco::u32 id = m_nextId++;
            auto window = std::make_unique<NullWindow>(id, settings);
            IWindow* borrowed = window.get();
            m_owned.push_back(std::move(window));
            m_live.push_back(borrowed);
            if (m_mainWindowId == 0) { m_mainWindowId = id; }  // the first window created is the main window
            return borrowed;
        }

        void destroyWindow(IWindow* window) override
        {
            if (!owns(window)) { return; }  // no-op for null or windows this manager does not own
            window->close();
            m_pendingDestroy.push_back(window->id());
        }

        [[nodiscard]] std::span<IWindow* const> windows() const noexcept override
        {
            return std::span<IWindow* const>(m_live.data(), m_live.size());
        }
        [[nodiscard]] IWindow* mainWindow() const noexcept override
        {
            // Tracked by id, so destroying/flushing the main window never promotes
            // another window into its place.
            return getWindow(m_mainWindowId);
        }
        [[nodiscard]] IWindow* getWindow(draco::u32 id) const noexcept override
        {
            for (IWindow* w : m_live) { if (w->id() == id) { return w; } }
            return nullptr;
        }
        [[nodiscard]] std::span<const WindowEvent> events() const noexcept override
        {
            return std::span<const WindowEvent>(m_events.data(), m_events.size());
        }

        void flushDestroyed() override
        {
            for (draco::u32 id : m_pendingDestroy)
            {
                for (draco::usize i = 0; i < m_live.size(); ++i)
                {
                    if (m_live[i]->id() == id) { m_live.erase(m_live.begin() + static_cast<std::ptrdiff_t>(i)); break; }
                }
                for (draco::usize i = 0; i < m_owned.size(); ++i)
                {
                    if (m_owned[i]->id() == id) { m_owned.erase(m_owned.begin() + static_cast<std::ptrdiff_t>(i)); break; }
                }
            }
            m_pendingDestroy.clear();
        }

    private:
        // True only for windows this manager owns (present in m_live). Rejects
        // nullptr too, so destroyWindow() is a no-op for null/unknown windows.
        // Checked by pointer identity, not id: a window from another manager can
        // share an id, and acting on it would corrupt this manager's bookkeeping.
        [[nodiscard]] bool owns(IWindow* window) const noexcept
        {
            for (IWindow* w : m_live) { if (w == window) { return true; } }
            return false;
        }

        std::vector<std::unique_ptr<NullWindow>> m_owned;
        std::vector<IWindow*> m_live;          // borrowed parallel pointers for the span
        std::vector<draco::u32> m_pendingDestroy; // window ids
        std::vector<WindowEvent> m_events;     // always empty (no OS event source)
        draco::u32 m_nextId = 1;
        draco::u32 m_mainWindowId = 0;         // id of the main window (first created); 0 = none
    };

    // No-op input devices: report nothing held/pressed so headless callers can
    // use input() uniformly without null checks.
    class NullKeyboard final : public IKeyboard
    {
    public:
        [[nodiscard]] bool isKeyDown(KeyCode) const override { return false; }
        [[nodiscard]] bool isKeyPressed(KeyCode) const override { return false; }
        [[nodiscard]] bool isKeyReleased(KeyCode) const override { return false; }
        [[nodiscard]] KeyModifiers modifiers() const override { return KeyModifiers::None; }
    };

    class NullMouse final : public IMouse
    {
    public:
        [[nodiscard]] draco::f32 x() const override { return 0.0f; }
        [[nodiscard]] draco::f32 y() const override { return 0.0f; }
        [[nodiscard]] draco::f32 deltaX() const override { return 0.0f; }
        [[nodiscard]] draco::f32 deltaY() const override { return 0.0f; }
        [[nodiscard]] draco::f32 scrollX() const override { return 0.0f; }
        [[nodiscard]] draco::f32 scrollY() const override { return 0.0f; }
        [[nodiscard]] bool isButtonDown(MouseButton) const override { return false; }
        [[nodiscard]] bool isButtonPressed(MouseButton) const override { return false; }
        [[nodiscard]] bool isButtonReleased(MouseButton) const override { return false; }
        [[nodiscard]] bool relativeMode() const override { return false; }
        void setRelativeMode(bool) override {}
        [[nodiscard]] bool cursorVisible() const override { return true; }
        void setCursorVisible(bool) override {}
        void setCursor(CursorType) override {}
    };

    class NullTouch final : public ITouch
    {
    public:
        [[nodiscard]] draco::i32 touchCount() const override { return 0; }
        [[nodiscard]] bool getTouchPoint(draco::i32, TouchPoint&) const override { return false; }
        [[nodiscard]] bool hasTouch() const override { return false; }
    };

    class NullInputManager final : public IInputManager
    {
    public:
        [[nodiscard]] IKeyboard* keyboard() override { return &m_keyboard; }
        [[nodiscard]] IMouse*    mouse()    override { return &m_mouse; }
        [[nodiscard]] ITouch*    touch()    override { return &m_touch; }
        [[nodiscard]] draco::i32 gamepadCount() const override { return 0; }
        [[nodiscard]] IGamepad*  getGamepad(draco::i32) override { return nullptr; }
        [[nodiscard]] std::span<const InputEvent> events() const override { return {}; }
        [[nodiscard]] draco::u32 hoverWindow()   const override { return 0; }
        [[nodiscard]] draco::u32 focusedWindow() const override { return 0; }
        void update() override {}

    private:
        NullKeyboard m_keyboard;
        NullMouse    m_mouse;
        NullTouch    m_touch;
    };

    class NullShell final : public IShell
    {
    public:
        explicit NullShell(const WindowSettings& settings = {}) noexcept : m_windows(settings) {}

        [[nodiscard]] IWindowManager* windowManager() noexcept override { return &m_windows; }
        [[nodiscard]] IWindow* mainWindow() noexcept override { return m_windows.mainWindow(); }
        [[nodiscard]] IInputManager* input() noexcept override { return &m_input; }
        void processEvents() override {}  // no OS event source
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
