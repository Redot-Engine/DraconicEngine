// shell:window - windowing types for the shell module: the window handle
// (IWindow), the manager that owns them (IWindowManager), the native OS handles
// RHI needs for surface creation, and the per-frame window event queue.
// Interface only; the null and SDL3 backends implement it.

module;

#include <expected>
#include <span>
#include <string_view>

export module shell:window;

export import core.stdtypes;

export namespace draco::shell
{
    // The windowing system a native handle belongs to (see NativeWindow).
    enum class WindowSystem : u8
    {
        Unknown,
        Win32,
        X11,
        Wayland,
        Cocoa,
    };

    // Native handles RHI needs to create its own surface/swapchain (RHI does
    // surface creation internally; the shell only hands over the handles). How the
    // pointers are interpreted depends on `system`:
    //   Win32:   display = HINSTANCE,   window = HWND
    //   X11:     display = Display*,     window = Window (XID, via uintptr)
    //   Wayland: display = wl_display*,  window = wl_surface*
    //   Cocoa:   display = nullptr,      window = NSWindow*
    struct NativeWindow
    {
        WindowSystem system = WindowSystem::Unknown;
        void* display = nullptr;
        void* window = nullptr;
    };

    // Parameters for creating a window (IWindowManager::createWindow).
    struct WindowSettings
    {
        std::u8string_view title = u8"Draconic";
        u32 width = 1280;
        u32 height = 720;
    };

    // A single OS window. Created and owned by IWindowManager; callers never
    // construct one directly.
    class IWindow
    {
    public:
        virtual ~IWindow() = default;

        // Stable per-window id, unique within a shell run. Routes OS events to the
        // right window and looks windows up. 0 is never a valid id.
        [[nodiscard]] virtual u32 id() const noexcept = 0;

        // Current client-area size in pixels.
        [[nodiscard]] virtual u32 width() const noexcept = 0;
        [[nodiscard]] virtual u32 height() const noexcept = 0;

        // Native OS handles for RHI surface creation (see NativeWindow).
        [[nodiscard]] virtual NativeWindow native() const noexcept = 0;

        // False once the window has been closed, by close() or an OS close event.
        [[nodiscard]] virtual bool isOpen() const noexcept = 0;

        // True while the window is minimized (renderers skip drawing). Resize is
        // detected by polling width()/height().
        [[nodiscard]] virtual bool isMinimized() const noexcept = 0;

        // Mark the window closed (isOpen() becomes false). Freeing it is the
        // manager's job, via destroyWindow()/flushDestroyed().
        virtual void close() = 0;
    };

    // What happened to a window during the last processEvents() pump. Delivered as
    // a per-frame queue (IWindowManager::events()) rather than a callback: this
    // matches the pull-based event model and avoids callback-lifetime concerns.
    // The consumer drains the queue each frame and reacts (resize a swapchain,
    // close a window, and so on).
    enum class WindowEventType : u8
    {
        Resized,
        Moved,
        FocusGained,
        FocusLost,
        CloseRequested,
    };

    // A single window event. Which of the fields below are meaningful depends on
    // `type` (noted per field).
    struct WindowEvent
    {
        WindowEventType type = WindowEventType::Resized;
        u32 windowId = 0;
        u32 width = 0;   // Resized
        u32 height = 0;  // Resized
        i32 x = 0;       // Moved
        i32 y = 0;       // Moved
    };

    // Error returned by IWindowManager::createWindow when a window cannot be made.
    // Placeholder for the engine's eventual Result error type; std::expected
    // requires a concrete error type.
    enum class WindowError : u8
    {
        CreationFailed,
    };

    // Owns the set of OS windows for a shell run (one manager per shell). The main
    // window is simply the first one created. Windows can be created and destroyed
    // at runtime, which is the basis for detachable/dockable UI windows.
    //
    // Destruction is deferred: destroyWindow() marks a window closed, and
    // flushDestroyed() (called at frame end, once the GPU is done with it) frees
    // it, so a window is never torn down mid-frame.
    class IWindowManager
    {
    public:
        virtual ~IWindowManager() = default;

        // Create a window. Returns a borrowed pointer owned by the manager, or a
        // WindowError on failure.
        [[nodiscard]] virtual std::expected<IWindow*, WindowError> createWindow(const WindowSettings& settings) = 0;

        // Mark a window for destruction at the next flushDestroyed(). Safe to call
        // mid-frame. No-op if the window is null or unknown.
        virtual void destroyWindow(IWindow* window) = 0;

        // All currently-live windows, in creation order, including
        // closed-but-not-yet-flushed ones until flushDestroyed() runs.
        [[nodiscard]] virtual std::span<IWindow* const> windows() const noexcept = 0;

        // The main window: the first window created, tracked by identity. Returns
        // null once it has been destroyed; it is never re-assigned to a different
        // window (closing the main window is not masked by other open windows).
        [[nodiscard]] virtual IWindow* mainWindow() const noexcept = 0;

        // Look up a live window by id, or null if there is no such window.
        [[nodiscard]] virtual IWindow* getWindow(u32 id) const noexcept = 0;

        // Window events accumulated during the last processEvents() pump. Valid
        // until the next pump; drained by the runner each frame.
        [[nodiscard]] virtual std::span<const WindowEvent> events() const noexcept = 0;

        // Free the windows previously marked by destroyWindow(). Call once per
        // frame at the end, after the GPU has finished the frame that used them.
        virtual void flushDestroyed() = 0;
    };
}
