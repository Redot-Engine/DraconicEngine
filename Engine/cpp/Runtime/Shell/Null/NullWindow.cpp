// shell.null:window implementation - the headless window + window manager.
module;

#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

module shell.null;

namespace draco::shell
{
    // ---- NullWindow ----

    NullWindow::NullWindow(u32 id, const WindowSettings& settings) noexcept
        : m_id(id), m_width(settings.width), m_height(settings.height) {}

    u32 NullWindow::id() const noexcept { return m_id; }
    u32 NullWindow::width() const noexcept { return m_width; }
    u32 NullWindow::height() const noexcept { return m_height; }
    NativeWindow NullWindow::native() const noexcept { return {}; }  // headless: no handles
    bool NullWindow::isOpen() const noexcept { return m_open; }
    bool NullWindow::isMinimized() const noexcept { return m_minimized; }
    void NullWindow::close() { m_open = false; }
    void NullWindow::resize(u32 w, u32 h) noexcept { m_width = w; m_height = h; }
    void NullWindow::setMinimized(bool m) noexcept { m_minimized = m; }

    // ---- NullWindowManager ----

    NullWindowManager::NullWindowManager(const WindowSettings& main) { (void)createWindow(main); }

    std::expected<IWindow*, WindowError> NullWindowManager::createWindow(const WindowSettings& settings)
    {
        const u32 id = m_nextId++;
        auto window = std::make_unique<NullWindow>(id, settings);
        IWindow* borrowed = window.get();
        m_owned.push_back(std::move(window));
        m_live.push_back(borrowed);
        if (m_mainWindowId == 0) { m_mainWindowId = id; }  // the first window created is the main window
        return borrowed;
    }

    void NullWindowManager::destroyWindow(IWindow* window)
    {
        if (!owns(window)) { return; }  // no-op for null or windows this manager does not own
        window->close();
        m_pendingDestroy.push_back(window->id());
    }

    std::span<IWindow* const> NullWindowManager::windows() const noexcept
    {
        return std::span<IWindow* const>(m_live.data(), m_live.size());
    }
    IWindow* NullWindowManager::mainWindow() const noexcept
    {
        // Tracked by id, so destroying/flushing the main window never promotes
        // another window into its place.
        return getWindow(m_mainWindowId);
    }
    IWindow* NullWindowManager::getWindow(u32 id) const noexcept
    {
        for (IWindow* w : m_live) { if (w->id() == id) { return w; } }
        return nullptr;
    }
    std::span<const WindowEvent> NullWindowManager::events() const noexcept
    {
        return std::span<const WindowEvent>(m_events.data(), m_events.size());
    }

    void NullWindowManager::flushDestroyed()
    {
        for (u32 id : m_pendingDestroy)
        {
            for (usize i = 0; i < m_live.size(); ++i)
            {
                if (m_live[i]->id() == id) { m_live.erase(m_live.begin() + static_cast<std::ptrdiff_t>(i)); break; }
            }
            for (usize i = 0; i < m_owned.size(); ++i)
            {
                if (m_owned[i]->id() == id) { m_owned.erase(m_owned.begin() + static_cast<std::ptrdiff_t>(i)); break; }
            }
        }
        m_pendingDestroy.clear();
    }

    bool NullWindowManager::owns(IWindow* window) const noexcept
    {
        for (IWindow* w : m_live) { if (w == window) { return true; } }
        return false;
    }
}
