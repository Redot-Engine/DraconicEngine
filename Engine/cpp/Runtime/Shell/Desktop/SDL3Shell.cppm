// shell.desktop - the desktop shell target (Windows/Linux/macOS), implemented on
// SDL3: SDL3Shell covers Wayland, X11, Win32, and Cocoa in one backend, plus
// input, clipboard, and native-handle export for RHI surface creation. We own the
// entry point (SDL_MAIN_HANDLED), so SDL does not hijack main; SDL_SetMainReady()
// is called before SDL_Init.
//
// If SDL video init or window creation fails (e.g. no display), the shell
// degrades: mainWindow() is null and isRunning() is false, so a runner exits
// immediately rather than crashing.

module;

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // SDL_SetMainReady (no main hijack with SDL_MAIN_HANDLED)

export module shell.desktop;

import core.stdtypes;
import shell;

export namespace draconic::shell
{
    class SDL3Window final : public IWindow
    {
    public:
        explicit SDL3Window(SDL_Window* window) noexcept : m_window(window)
        {
            int w = 0, h = 0;
            SDL_GetWindowSize(m_window, &w, &h);
            m_width = static_cast<draco::u32>(w);
            m_height = static_cast<draco::u32>(h);
            m_id = static_cast<draco::u32>(SDL_GetWindowID(m_window));
        }

        ~SDL3Window() override { if (m_window != nullptr) { SDL_DestroyWindow(m_window); } }

        SDL3Window(const SDL3Window&) = delete;
        SDL3Window& operator=(const SDL3Window&) = delete;

        [[nodiscard]] draco::u32 id() const noexcept override { return m_id; }
        [[nodiscard]] draco::u32 width() const noexcept override { return m_width; }
        [[nodiscard]] draco::u32 height() const noexcept override { return m_height; }

        // Extract the real native handles from SDL's window properties so RHI can
        // create its own surface (it does not use SDL's Vulkan helpers).
        [[nodiscard]] NativeWindow native() const noexcept override
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

        [[nodiscard]] bool isOpen() const noexcept override { return m_open; }
        [[nodiscard]] bool isMinimized() const noexcept override
        {
            return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) != 0;
        }
        void close() override { m_open = false; }

        [[nodiscard]] SDL_Window* handle() const noexcept { return m_window; }
        void onResized(draco::u32 w, draco::u32 h) noexcept { m_width = w; m_height = h; }

    private:
        SDL_Window* m_window;
        draco::u32 m_id = 0;
        draco::u32 m_width = 0;
        draco::u32 m_height = 0;
        bool m_open = true;
    };

    // Builds SDL window-creation flags. On Wayland a Vulkan-backed window is
    // needed for client-side decorations (see SDL3Shell ctor note); skipped
    // under the headless "dummy" driver so tests still get a window.
    [[nodiscard]] inline SDL_WindowFlags sdl3WindowFlags() noexcept
    {
        SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
#if defined(__linux__)
        const char* driver = SDL_GetCurrentVideoDriver();
        if (driver != nullptr && SDL_strcmp(driver, "dummy") != 0) { flags |= SDL_WINDOW_VULKAN; }
#endif
        return flags;
    }

    // Owns the SDL windows for the run. The main window is the first created.
    // Window destruction is deferred to flushDestroyed() so a window is never
    // freed mid-frame while the GPU may still reference its swapchain.
    class SDL3WindowManager final : public IWindowManager
    {
    public:
        [[nodiscard]] std::expected<IWindow*, WindowError> createWindow(const WindowSettings& settings) override
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

        void destroyWindow(IWindow* window) override
        {
            if (!owns(window)) { return; }  // no-op for null or windows this manager does not own
            window->close();
            m_pendingDestroy.push_back(window->id());
        }

        [[nodiscard]] std::span<IWindow* const> windows() noexcept override
        {
            return std::span<IWindow* const>(m_live.data(), m_live.size());
        }
        [[nodiscard]] IWindow* mainWindow() noexcept override
        {
            // Tracked by id, so destroying/flushing the main window never promotes
            // another window into its place.
            return getWindow(m_mainWindowId);
        }
        [[nodiscard]] IWindow* getWindow(draco::u32 id) noexcept override
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
                    if (m_owned[i]->id() == id) { m_owned.erase(m_owned.begin() + static_cast<std::ptrdiff_t>(i)); break; }  // dtor destroys SDL window
                }
            }
            m_pendingDestroy.clear();
        }

        // --- event pump wiring (called by SDL3Shell::processEvents) ---
        SDL3Window* find(draco::u32 id) noexcept
        {
            for (std::unique_ptr<SDL3Window>& w : m_owned) { if (w->id() == id) { return w.get(); } }
            return nullptr;
        }
        void clearEvents() noexcept { m_events.clear(); }
        void pushEvent(const WindowEvent& e) { m_events.push_back(e); }

        // Destroy every window immediately (SDL3Window dtors call
        // SDL_DestroyWindow). The shell calls this before SDL_Quit().
        void destroyAllNow()
        {
            m_live.clear();
            m_owned.clear();
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

        std::vector<std::unique_ptr<SDL3Window>> m_owned;
        std::vector<IWindow*> m_live;
        std::vector<draco::u32> m_pendingDestroy;
        std::vector<WindowEvent> m_events;
        draco::u32 m_mainWindowId = 0;   // id of the main window (first created); 0 = none
    };

    // -----------------------------------------------------------------------
    // Input devices - double-buffered state fed by the SDL3 event pump.
    // -----------------------------------------------------------------------
    inline constexpr draco::u32 kKeyCount           = static_cast<draco::u32>(KeyCode::Count);
    inline constexpr draco::u32 kMouseButtonCount   = static_cast<draco::u32>(MouseButton::Count);
    inline constexpr draco::u32 kGamepadButtonCount = static_cast<draco::u32>(GamepadButton::Count);
    inline constexpr draco::u32 kCursorCount        = static_cast<draco::u32>(CursorType::Count);

    class SDL3Keyboard final : public IKeyboard
    {
    public:
        [[nodiscard]] bool isKeyDown(KeyCode key) const override { return m_current[index(key)]; }
        [[nodiscard]] bool isKeyPressed(KeyCode key) const override
        {
            const draco::u32 i = index(key);
            return m_current[i] && !m_previous[i];
        }
        [[nodiscard]] bool isKeyReleased(KeyCode key) const override
        {
            const draco::u32 i = index(key);
            return !m_current[i] && m_previous[i];
        }
        [[nodiscard]] KeyModifiers modifiers() const override { return m_mods; }

        void setKey(KeyCode key, bool down) { m_current[index(key)] = down; }
        void setModifiers(KeyModifiers mods) { m_mods = mods; }
        void beginFrame() { for (draco::u32 i = 0; i < kKeyCount; ++i) { m_previous[i] = m_current[i]; } }

    private:
        static draco::u32 index(KeyCode key) noexcept
        {
            const draco::u32 i = static_cast<draco::u32>(key);
            return i < kKeyCount ? i : 0;
        }
        bool m_current[kKeyCount] = {};
        bool m_previous[kKeyCount] = {};
        KeyModifiers m_mods = KeyModifiers::None;
    };

    class SDL3Mouse final : public IMouse
    {
    public:
        [[nodiscard]] draco::f32 x() const override { return m_x; }
        [[nodiscard]] draco::f32 y() const override { return m_y; }
        [[nodiscard]] draco::f32 deltaX() const override { return m_dx; }
        [[nodiscard]] draco::f32 deltaY() const override { return m_dy; }
        [[nodiscard]] draco::f32 scrollX() const override { return m_sx; }
        [[nodiscard]] draco::f32 scrollY() const override { return m_sy; }
        [[nodiscard]] bool isButtonDown(MouseButton b) const override { return m_current[index(b)]; }
        [[nodiscard]] bool isButtonPressed(MouseButton b) const override
        {
            const draco::u32 i = index(b);
            return m_current[i] && !m_previous[i];
        }
        [[nodiscard]] bool isButtonReleased(MouseButton b) const override
        {
            const draco::u32 i = index(b);
            return !m_current[i] && m_previous[i];
        }
        [[nodiscard]] bool relativeMode() const override { return m_relative; }
        void setRelativeMode(bool enabled) override
        {
            if (m_window != nullptr) { SDL_SetWindowRelativeMouseMode(m_window, enabled); }
            m_relative = enabled;
        }
        [[nodiscard]] bool cursorVisible() const override { return m_cursorVisible; }
        void setCursorVisible(bool visible) override
        {
            if (visible) { SDL_ShowCursor(); } else { SDL_HideCursor(); }
            m_cursorVisible = visible;
        }
        void setCursor(CursorType cursor) override
        {
            const draco::u32 i = static_cast<draco::u32>(cursor);
            if (i >= kCursorCount) { return; }
            if (m_cursors[i] == nullptr) { m_cursors[i] = SDL_CreateSystemCursor(mapSystemCursor(cursor)); }
            if (m_cursors[i] != nullptr) { SDL_SetCursor(m_cursors[i]); m_cursor = cursor; }
        }

        void setWindow(SDL_Window* window) { m_window = window; }
        // Frees the lazily-created system cursors. Called before SDL_Quit so no
        // SDL calls happen after the video subsystem is torn down.
        void releaseCursors()
        {
            for (SDL_Cursor*& c : m_cursors)
            {
                if (c != nullptr) { SDL_DestroyCursor(c); c = nullptr; }
            }
        }
        void onMotion(draco::f32 x, draco::f32 y, draco::f32 relX, draco::f32 relY)
        {
            m_x = x; m_y = y; m_dx += relX; m_dy += relY;
        }
        void onButton(draco::u32 idx, bool down) { if (idx < kMouseButtonCount) { m_current[idx] = down; } }
        void onWheel(draco::f32 x, draco::f32 y) { m_sx += x; m_sy += y; }
        void beginFrame()
        {
            for (draco::u32 i = 0; i < kMouseButtonCount; ++i) { m_previous[i] = m_current[i]; }
            m_dx = m_dy = m_sx = m_sy = 0.0f;
        }

    private:
        static draco::u32 index(MouseButton b) noexcept
        {
            const draco::u32 i = static_cast<draco::u32>(b);
            return i < kMouseButtonCount ? i : 0;
        }

        static SDL_SystemCursor mapSystemCursor(CursorType cursor) noexcept
        {
            switch (cursor)
            {
                case CursorType::Default:    return SDL_SYSTEM_CURSOR_DEFAULT;
                case CursorType::Text:       return SDL_SYSTEM_CURSOR_TEXT;
                case CursorType::Wait:       return SDL_SYSTEM_CURSOR_WAIT;
                case CursorType::Crosshair:  return SDL_SYSTEM_CURSOR_CROSSHAIR;
                case CursorType::Progress:   return SDL_SYSTEM_CURSOR_PROGRESS;
                case CursorType::ResizeNWSE: return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
                case CursorType::ResizeNESW: return SDL_SYSTEM_CURSOR_NESW_RESIZE;
                case CursorType::ResizeEW:   return SDL_SYSTEM_CURSOR_EW_RESIZE;
                case CursorType::ResizeNS:   return SDL_SYSTEM_CURSOR_NS_RESIZE;
                case CursorType::ResizeNW:   return SDL_SYSTEM_CURSOR_NW_RESIZE;
                case CursorType::ResizeN:    return SDL_SYSTEM_CURSOR_N_RESIZE;
                case CursorType::ResizeNE:   return SDL_SYSTEM_CURSOR_NE_RESIZE;
                case CursorType::ResizeE:    return SDL_SYSTEM_CURSOR_E_RESIZE;
                case CursorType::ResizeSE:   return SDL_SYSTEM_CURSOR_SE_RESIZE;
                case CursorType::ResizeS:    return SDL_SYSTEM_CURSOR_S_RESIZE;
                case CursorType::ResizeSW:   return SDL_SYSTEM_CURSOR_SW_RESIZE;
                case CursorType::ResizeW:    return SDL_SYSTEM_CURSOR_W_RESIZE;
                case CursorType::Move:       return SDL_SYSTEM_CURSOR_MOVE;
                case CursorType::NotAllowed: return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
                case CursorType::Pointer:    return SDL_SYSTEM_CURSOR_POINTER;
                default:                     return SDL_SYSTEM_CURSOR_DEFAULT;
            }
        }

        SDL_Window* m_window = nullptr;
        draco::f32 m_x = 0, m_y = 0, m_dx = 0, m_dy = 0, m_sx = 0, m_sy = 0;
        bool m_current[kMouseButtonCount] = {};
        bool m_previous[kMouseButtonCount] = {};
        bool m_relative = false;
        bool m_cursorVisible = true;
        CursorType m_cursor = CursorType::Default;
        SDL_Cursor* m_cursors[kCursorCount] = {};  // lazily created, cached
    };

    class SDL3Gamepad final : public IGamepad
    {
    public:
        SDL3Gamepad(SDL_Gamepad* pad, SDL_JoystickID id, draco::i32 index, std::u8string name) noexcept
            : m_pad(pad), m_id(id), m_index(index), m_name(std::move(name)) {}

        [[nodiscard]] draco::i32 index() const override { return m_index; }
        [[nodiscard]] std::u8string_view name() const override { return m_name; }
        [[nodiscard]] bool connected() const override { return m_pad != nullptr; }
        [[nodiscard]] bool isButtonDown(GamepadButton b) const override { return m_current[buttonIndex(b)]; }
        [[nodiscard]] bool isButtonPressed(GamepadButton b) const override
        {
            const draco::u32 i = buttonIndex(b);
            return m_current[i] && !m_previous[i];
        }
        [[nodiscard]] bool isButtonReleased(GamepadButton b) const override
        {
            const draco::u32 i = buttonIndex(b);
            return !m_current[i] && m_previous[i];
        }
        [[nodiscard]] draco::f32 axis(GamepadAxis a) const override
        {
            if (m_pad == nullptr) { return 0.0f; }
            const auto raw = SDL_GetGamepadAxis(m_pad, static_cast<SDL_GamepadAxis>(static_cast<draco::u32>(a)));
            return static_cast<draco::f32>(raw) / 32767.0f;
        }
        void setRumble(draco::f32 lowFreq, draco::f32 highFreq, draco::u32 durationMs) override
        {
            if (m_pad != nullptr)
            {
                SDL_RumbleGamepad(m_pad,
                                  static_cast<draco::u16>(lowFreq * 65535.0f),
                                  static_cast<draco::u16>(highFreq * 65535.0f),
                                  durationMs);
            }
        }

        [[nodiscard]] SDL_JoystickID joystickId() const noexcept { return m_id; }
        [[nodiscard]] SDL_Gamepad* handle() const noexcept { return m_pad; }
        void setIndex(draco::i32 index) noexcept { m_index = index; }
        void setButton(GamepadButton b, bool down) { m_current[buttonIndex(b)] = down; }
        void disconnect() noexcept { m_pad = nullptr; }
        void beginFrame() { for (draco::u32 i = 0; i < kGamepadButtonCount; ++i) { m_previous[i] = m_current[i]; } }

    private:
        static draco::u32 buttonIndex(GamepadButton b) noexcept
        {
            const draco::u32 i = static_cast<draco::u32>(b);
            return i < kGamepadButtonCount ? i : 0;
        }
        SDL_Gamepad* m_pad;
        SDL_JoystickID m_id;
        draco::i32 m_index;
        std::u8string m_name;
        bool m_current[kGamepadButtonCount] = {};
        bool m_previous[kGamepadButtonCount] = {};
    };

    class SDL3Touch final : public ITouch
    {
    public:
        [[nodiscard]] draco::i32 touchCount() const override { return static_cast<draco::i32>(m_points.size()); }
        [[nodiscard]] bool getTouchPoint(draco::i32 index, TouchPoint& out) const override
        {
            if (index < 0 || static_cast<draco::usize>(index) >= m_points.size()) { return false; }
            out = m_points[static_cast<draco::usize>(index)];
            return true;
        }
        [[nodiscard]] bool hasTouch() const override { return !m_points.empty(); }

        void addOrUpdate(const TouchPoint& tp)
        {
            for (draco::usize i = 0; i < m_points.size(); ++i)
            {
                if (m_points[i].id == tp.id) { m_points[i] = tp; return; }
            }
            m_points.push_back(tp);
        }
        void remove(draco::u64 id)
        {
            for (draco::usize i = 0; i < m_points.size(); ++i)
            {
                if (m_points[i].id == id)
                {
                    m_points[i] = m_points.back();  // swap-and-pop; order does not matter
                    m_points.pop_back();
                    return;
                }
            }
        }

    private:
        std::vector<TouchPoint> m_points;
    };

    class SDL3InputManager final : public IInputManager
    {
    public:
        ~SDL3InputManager() override { releaseDevices(); }

        // Frees all SDL-owned input resources (open gamepads, system cursors).
        // The shell calls this before SDL_Quit; idempotent so the destructor
        // can call it again harmlessly.
        void releaseDevices()
        {
            for (SDL3Gamepad* g : m_gamepads)
            {
                if (g->handle() != nullptr) { SDL_CloseGamepad(g->handle()); }
                delete g;
            }
            m_gamepads.clear();
            m_mouse.releaseCursors();
        }

        [[nodiscard]] IKeyboard* keyboard() override { return &m_keyboard; }
        [[nodiscard]] IMouse*    mouse()    override { return &m_mouse; }
        [[nodiscard]] ITouch*    touch()    override { return &m_touch; }
        [[nodiscard]] draco::i32 gamepadCount() const override { return static_cast<draco::i32>(m_gamepads.size()); }
        [[nodiscard]] IGamepad*  getGamepad(draco::i32 index) override
        {
            if (index < 0 || static_cast<draco::usize>(index) >= m_gamepads.size()) { return nullptr; }
            return m_gamepads[static_cast<draco::usize>(index)];
        }
        [[nodiscard]] std::span<const InputEvent> events() const override
        {
            return std::span<const InputEvent>{ m_events.data(), m_events.size() };
        }
        [[nodiscard]] draco::u32 hoverWindow()   const override { return m_hoverWindow; }
        [[nodiscard]] draco::u32 focusedWindow() const override { return m_focusWindow; }
        void update() override
        {
            m_keyboard.beginFrame();
            m_mouse.beginFrame();
            for (SDL3Gamepad* g : m_gamepads) { g->beginFrame(); }
            m_events.clear();   // events are valid only for the frame they were pumped in
        }

        // --- backend wiring (called by the shell event pump) ---
        SDL3Keyboard& keyboardDevice() noexcept { return m_keyboard; }
        SDL3Mouse&    mouseDevice() noexcept { return m_mouse; }
        SDL3Touch&    touchDevice() noexcept { return m_touch; }
        void setWindow(SDL_Window* window) { m_mouse.setWindow(window); }

        // Emit an input event onto this frame's stream (also apply it to the snapshot at the
        // call site; the snapshot is a fold over these events).
        void emitEvent(const InputEvent& e) { m_events.push_back(e); }
        void setHoverWindow(draco::u32 id)   noexcept { m_hoverWindow = id; }
        void setFocusWindow(draco::u32 id)   noexcept { m_focusWindow = id; }

        void addGamepad(SDL_JoystickID id)
        {
            if (findGamepadById(id) != nullptr) { return; }
            SDL_Gamepad* pad = SDL_OpenGamepad(id);
            if (pad == nullptr) { return; }

            const char* nm = SDL_GetGamepadName(pad);
            std::u8string name = (nm != nullptr)
                ? std::u8string(reinterpret_cast<const char8_t*>(nm))
                : std::u8string{};
            const draco::i32 index = static_cast<draco::i32>(m_gamepads.size());
            m_gamepads.push_back(new SDL3Gamepad(pad, id, index, std::move(name)));
        }

        void removeGamepad(SDL_JoystickID id)
        {
            for (draco::usize i = 0; i < m_gamepads.size(); ++i)
            {
                if (m_gamepads[i]->joystickId() == id)
                {
                    if (m_gamepads[i]->handle() != nullptr) { SDL_CloseGamepad(m_gamepads[i]->handle()); }
                    delete m_gamepads[i];
                    m_gamepads.erase(m_gamepads.begin() + static_cast<std::ptrdiff_t>(i));
                    for (draco::usize j = 0; j < m_gamepads.size(); ++j) { m_gamepads[j]->setIndex(static_cast<draco::i32>(j)); }
                    return;
                }
            }
        }

        SDL3Gamepad* findGamepadById(SDL_JoystickID id)
        {
            for (SDL3Gamepad* g : m_gamepads) { if (g->joystickId() == id) { return g; } }
            return nullptr;
        }

    private:
        SDL3Keyboard m_keyboard;
        SDL3Mouse    m_mouse;
        SDL3Touch    m_touch;
        std::vector<SDL3Gamepad*> m_gamepads;
        std::vector<InputEvent>   m_events;         // this frame's event stream
        draco::u32                m_hoverWindow = 0; // window under the pointer
        draco::u32                m_focusWindow = 0; // keyboard-focused window
    };

    class SDL3Shell final : public IShell
    {
    public:
        // Note on the Wayland Vulkan-window quirk: SDL only attaches libdecor
        // client-side decorations to a window backed by a GPU surface, so a plain
        // window comes up bare on GNOME/Mutter. sdl3WindowFlags() flags every
        // window as Vulkan on Linux (skipped under the "dummy" driver) to fix it.
        explicit SDL3Shell(const WindowSettings& settings = {}) noexcept
        {
            SDL_SetMainReady();
            if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { m_running = false; return; }
            m_initialized = true;

            std::expected<IWindow*, WindowError> main = m_windows.createWindow(settings);
            if (!main.has_value()) { m_running = false; return; }
            if (SDL3Window* w = m_windows.find(main.value()->id())) { m_input.setWindow(w->handle()); }
        }

        ~SDL3Shell() override
        {
            // Release SDL-owned input resources and destroy windows before
            // tearing SDL down (no SDL calls may happen after SDL_Quit).
            m_input.releaseDevices();
            m_windows.destroyAllNow();
            if (m_initialized) { SDL_Quit(); }
        }

        SDL3Shell(const SDL3Shell&) = delete;
        SDL3Shell& operator=(const SDL3Shell&) = delete;

        [[nodiscard]] IWindowManager* windowManager() noexcept override { return &m_windows; }
        [[nodiscard]] IWindow* mainWindow() noexcept override { return m_windows.mainWindow(); }
        [[nodiscard]] IInputManager* input() noexcept override { return &m_input; }

        void processEvents() override
        {
            // Roll input state (current -> previous, clear deltas) before pumping;
            // clear last frame's window events (they're valid only until now).
            m_input.update();
            m_windows.clearEvents();

            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                    case SDL_EVENT_QUIT:
                        // App-level quit: close the main window and stop the loop.
                        if (IWindow* main = m_windows.mainWindow())
                        {
                            main->close();
                            m_windows.pushEvent(WindowEvent{ WindowEventType::CloseRequested, main->id() });
                        }
                        m_running = false;
                        break;
                    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                    {
                        const draco::u32 id = static_cast<draco::u32>(event.window.windowID);
                        m_windows.pushEvent(WindowEvent{ WindowEventType::CloseRequested, id });
                        // Closing the main window stops the shell; the runner
                        // handles secondary-window close via the event queue.
                        IWindow* main = m_windows.mainWindow();
                        if (main != nullptr && main->id() == id) { main->close(); m_running = false; }
                        break;
                    }
                    case SDL_EVENT_WINDOW_RESIZED:
                    {
                        const draco::u32 id = static_cast<draco::u32>(event.window.windowID);
                        if (SDL3Window* w = m_windows.find(id))
                        {
                            const draco::u32 nw = static_cast<draco::u32>(event.window.data1);
                            const draco::u32 nh = static_cast<draco::u32>(event.window.data2);
                            w->onResized(nw, nh);
                            m_windows.pushEvent(WindowEvent{ WindowEventType::Resized, id, nw, nh });
                        }
                        break;
                    }
                    case SDL_EVENT_WINDOW_FOCUS_GAINED:
                    {
                        const draco::u32 id = static_cast<draco::u32>(event.window.windowID);
                        m_input.setFocusWindow(id);   // keyboard/gamepad routing authority
                        m_windows.pushEvent(WindowEvent{ WindowEventType::FocusGained, id });
                        break;
                    }
                    case SDL_EVENT_WINDOW_FOCUS_LOST:
                    {
                        const draco::u32 id = static_cast<draco::u32>(event.window.windowID);
                        if (m_input.focusedWindow() == id) { m_input.setFocusWindow(0); }
                        m_windows.pushEvent(WindowEvent{ WindowEventType::FocusLost, id });
                        break;
                    }
                    case SDL_EVENT_WINDOW_MOUSE_ENTER:
                        m_input.setHoverWindow(static_cast<draco::u32>(event.window.windowID));
                        break;
                    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
                        if (m_input.hoverWindow() == static_cast<draco::u32>(event.window.windowID))
                        {
                            m_input.setHoverWindow(0);
                        }
                        break;

                    // --- Keyboard --- (emit event, then fold into the snapshot)
                    case SDL_EVENT_KEY_DOWN:
                    case SDL_EVENT_KEY_UP:
                    {
                        InputEvent e{};
                        e.kind = event.key.down ? InputEventKind::KeyDown : InputEventKind::KeyUp;
                        e.window = static_cast<draco::u32>(event.key.windowID);
                        e.key = mapKeyCode(event.key.scancode);
                        e.modifiers = mapModifiers(event.key.mod);
                        m_input.emitEvent(e);
                        m_input.keyboardDevice().setKey(e.key, event.key.down);
                        m_input.keyboardDevice().setModifiers(e.modifiers);
                        break;
                    }
                    case SDL_EVENT_TEXT_INPUT:   // only arrives after SDL_StartTextInput (focus-driven, later)
                    {
                        InputEvent e{};
                        e.kind = InputEventKind::TextInput;
                        e.window = static_cast<draco::u32>(event.text.windowID);
                        if (event.text.text != nullptr)
                        {
                            draco::usize n = 0;
                            while (n + 1 < sizeof(e.text) && event.text.text[n] != '\0')
                            {
                                e.text[n] = static_cast<char8_t>(event.text.text[n]); ++n;
                            }
                            e.text[n] = static_cast<char8_t>('\0');
                        }
                        m_input.emitEvent(e);
                        break;
                    }

                    // --- Mouse ---
                    case SDL_EVENT_MOUSE_MOTION:
                    {
                        m_input.setHoverWindow(static_cast<draco::u32>(event.motion.windowID));
                        InputEvent e{};
                        e.kind = InputEventKind::MouseMove;
                        e.window = static_cast<draco::u32>(event.motion.windowID);
                        e.x = event.motion.x; e.y = event.motion.y;
                        e.dx = event.motion.xrel; e.dy = event.motion.yrel;
                        m_input.emitEvent(e);
                        m_input.mouseDevice().onMotion(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
                        break;
                    }
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    case SDL_EVENT_MOUSE_BUTTON_UP:
                    {
                        const draco::u32 btn = static_cast<draco::u32>(event.button.button) - 1;
                        const bool down = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                        InputEvent e{};
                        e.kind = down ? InputEventKind::MouseButtonDown : InputEventKind::MouseButtonUp;
                        e.window = static_cast<draco::u32>(event.button.windowID);
                        e.button = mapMouseButton(btn);
                        e.x = event.button.x; e.y = event.button.y;
                        m_input.emitEvent(e);
                        m_input.mouseDevice().onButton(btn, down);
                        break;
                    }
                    case SDL_EVENT_MOUSE_WHEEL:
                    {
                        InputEvent e{};
                        e.kind = InputEventKind::MouseWheel;
                        e.window = static_cast<draco::u32>(event.wheel.windowID);
                        e.x = event.wheel.x; e.y = event.wheel.y;
                        m_input.emitEvent(e);
                        m_input.mouseDevice().onWheel(event.wheel.x, event.wheel.y);
                        break;
                    }

                    // --- Touch ---
                    case SDL_EVENT_FINGER_DOWN:
                    case SDL_EVENT_FINGER_MOTION:
                    {
                        InputEvent e{};
                        e.kind = (event.type == SDL_EVENT_FINGER_DOWN) ? InputEventKind::TouchDown : InputEventKind::TouchMove;
                        e.window = static_cast<draco::u32>(event.tfinger.windowID);
                        e.touchId = static_cast<draco::u64>(event.tfinger.fingerID);
                        e.x = event.tfinger.x; e.y = event.tfinger.y; e.value = event.tfinger.pressure;
                        m_input.emitEvent(e);
                        m_input.touchDevice().addOrUpdate(TouchPoint{ e.touchId, e.x, e.y, e.value });
                        break;
                    }
                    case SDL_EVENT_FINGER_UP:
                    {
                        InputEvent e{};
                        e.kind = InputEventKind::TouchUp;
                        e.window = static_cast<draco::u32>(event.tfinger.windowID);
                        e.touchId = static_cast<draco::u64>(event.tfinger.fingerID);
                        e.x = event.tfinger.x; e.y = event.tfinger.y;
                        m_input.emitEvent(e);
                        m_input.touchDevice().remove(e.touchId);
                        break;
                    }

                    // --- Gamepad --- (tagged with the focused window; pads aren't window-bound)
                    case SDL_EVENT_GAMEPAD_ADDED:
                        m_input.addGamepad(event.gdevice.which);
                        break;
                    case SDL_EVENT_GAMEPAD_REMOVED:
                        m_input.removeGamepad(event.gdevice.which);
                        break;
                    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                    case SDL_EVENT_GAMEPAD_BUTTON_UP:
                        if (SDL3Gamepad* pad = m_input.findGamepadById(event.gbutton.which))
                        {
                            const GamepadButton b = mapGamepadButton(static_cast<SDL_GamepadButton>(event.gbutton.button));
                            if (b != GamepadButton::Count)
                            {
                                const bool down = (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                                InputEvent e{};
                                e.kind = down ? InputEventKind::GamepadButtonDown : InputEventKind::GamepadButtonUp;
                                e.window = m_input.focusedWindow();
                                e.gamepad = pad->index(); e.padButton = b;
                                m_input.emitEvent(e);
                                pad->setButton(b, down);
                            }
                        }
                        break;
                    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                        if (SDL3Gamepad* pad = m_input.findGamepadById(event.gaxis.which))
                        {
                            const GamepadAxis a = mapGamepadAxis(static_cast<SDL_GamepadAxis>(event.gaxis.axis));
                            if (a != GamepadAxis::Count)
                            {
                                InputEvent e{};
                                e.kind = InputEventKind::GamepadAxis;
                                e.window = m_input.focusedWindow();
                                e.gamepad = pad->index(); e.padAxis = a;
                                e.value = static_cast<draco::f32>(event.gaxis.value) / 32767.0f;   // snapshot reads axes live
                                m_input.emitEvent(e);
                            }
                        }
                        break;

                    default:
                        break;
                }
            }
        }

        [[nodiscard]] bool isRunning() const noexcept override
        {
            IWindow* main = const_cast<SDL3WindowManager&>(m_windows).mainWindow();
            return m_running && main != nullptr && main->isOpen();
        }

        void requestExit() override { m_running = false; }

    private:
        static KeyCode mapKeyCode(SDL_Scancode sc) noexcept
        {
            if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
                return static_cast<KeyCode>(static_cast<draco::u32>(KeyCode::A) + (sc - SDL_SCANCODE_A));
            if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
                return static_cast<KeyCode>(static_cast<draco::u32>(KeyCode::Num1) + (sc - SDL_SCANCODE_1));
            if (sc == SDL_SCANCODE_0) return KeyCode::Num0;
            if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12)
                return static_cast<KeyCode>(static_cast<draco::u32>(KeyCode::F1) + (sc - SDL_SCANCODE_F1));
            switch (sc)
            {
                case SDL_SCANCODE_RETURN:    return KeyCode::Return;
                case SDL_SCANCODE_ESCAPE:    return KeyCode::Escape;
                case SDL_SCANCODE_BACKSPACE: return KeyCode::Backspace;
                case SDL_SCANCODE_TAB:       return KeyCode::Tab;
                case SDL_SCANCODE_SPACE:     return KeyCode::Space;
                case SDL_SCANCODE_UP:        return KeyCode::Up;
                case SDL_SCANCODE_DOWN:      return KeyCode::Down;
                case SDL_SCANCODE_LEFT:      return KeyCode::Left;
                case SDL_SCANCODE_RIGHT:     return KeyCode::Right;
                case SDL_SCANCODE_LCTRL:     return KeyCode::LeftCtrl;
                case SDL_SCANCODE_LSHIFT:    return KeyCode::LeftShift;
                case SDL_SCANCODE_LALT:      return KeyCode::LeftAlt;
                case SDL_SCANCODE_LGUI:      return KeyCode::LeftGui;
                case SDL_SCANCODE_RCTRL:     return KeyCode::RightCtrl;
                case SDL_SCANCODE_RSHIFT:    return KeyCode::RightShift;
                case SDL_SCANCODE_RALT:      return KeyCode::RightAlt;
                case SDL_SCANCODE_RGUI:      return KeyCode::RightGui;
                case SDL_SCANCODE_DELETE:    return KeyCode::Delete;
                case SDL_SCANCODE_INSERT:    return KeyCode::Insert;
                case SDL_SCANCODE_HOME:      return KeyCode::Home;
                case SDL_SCANCODE_END:       return KeyCode::End;
                case SDL_SCANCODE_PAGEUP:    return KeyCode::PageUp;
                case SDL_SCANCODE_PAGEDOWN:  return KeyCode::PageDown;
                default:                     return KeyCode::Unknown;
            }
        }

        static KeyModifiers mapModifiers(SDL_Keymod mod) noexcept
        {
            KeyModifiers m = KeyModifiers::None;
            if (mod & SDL_KMOD_LSHIFT) { m |= KeyModifiers::LeftShift; }
            if (mod & SDL_KMOD_RSHIFT) { m |= KeyModifiers::RightShift; }
            if (mod & SDL_KMOD_LCTRL)  { m |= KeyModifiers::LeftCtrl; }
            if (mod & SDL_KMOD_RCTRL)  { m |= KeyModifiers::RightCtrl; }
            if (mod & SDL_KMOD_LALT)   { m |= KeyModifiers::LeftAlt; }
            if (mod & SDL_KMOD_RALT)   { m |= KeyModifiers::RightAlt; }
            if (mod & SDL_KMOD_LGUI)   { m |= KeyModifiers::LeftGui; }
            if (mod & SDL_KMOD_RGUI)   { m |= KeyModifiers::RightGui; }
            if (mod & SDL_KMOD_NUM)    { m |= KeyModifiers::NumLock; }
            if (mod & SDL_KMOD_CAPS)   { m |= KeyModifiers::CapsLock; }
            if (mod & SDL_KMOD_SCROLL) { m |= KeyModifiers::ScrollLock; }
            return m;
        }

        // SDL's button order differs from ours, so map explicitly.
        static GamepadButton mapGamepadButton(SDL_GamepadButton b) noexcept
        {
            switch (b)
            {
                case SDL_GAMEPAD_BUTTON_SOUTH:          return GamepadButton::South;
                case SDL_GAMEPAD_BUTTON_EAST:           return GamepadButton::East;
                case SDL_GAMEPAD_BUTTON_WEST:           return GamepadButton::West;
                case SDL_GAMEPAD_BUTTON_NORTH:          return GamepadButton::North;
                case SDL_GAMEPAD_BUTTON_BACK:           return GamepadButton::Back;
                case SDL_GAMEPAD_BUTTON_GUIDE:          return GamepadButton::Guide;
                case SDL_GAMEPAD_BUTTON_START:          return GamepadButton::Start;
                case SDL_GAMEPAD_BUTTON_LEFT_STICK:     return GamepadButton::LeftStick;
                case SDL_GAMEPAD_BUTTON_RIGHT_STICK:    return GamepadButton::RightStick;
                case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return GamepadButton::LeftShoulder;
                case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return GamepadButton::RightShoulder;
                case SDL_GAMEPAD_BUTTON_DPAD_UP:        return GamepadButton::DPadUp;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return GamepadButton::DPadDown;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return GamepadButton::DPadLeft;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return GamepadButton::DPadRight;
                case SDL_GAMEPAD_BUTTON_MISC1:          return GamepadButton::Misc1;
                case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:   return GamepadButton::LeftPaddle1;
                case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:   return GamepadButton::LeftPaddle2;
                case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:  return GamepadButton::RightPaddle1;
                case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:  return GamepadButton::RightPaddle2;
                case SDL_GAMEPAD_BUTTON_TOUCHPAD:       return GamepadButton::Touchpad;
                default:                                return GamepadButton::Count;  // unmapped
            }
        }

        // SDL mouse button number (1-based) minus 1 -> MouseButton (Left/Middle/Right/X1/X2).
        static MouseButton mapMouseButton(draco::u32 idx) noexcept
        {
            switch (idx)
            {
                case 0: return MouseButton::Left;
                case 1: return MouseButton::Middle;
                case 2: return MouseButton::Right;
                case 3: return MouseButton::X1;
                case 4: return MouseButton::X2;
                default: return MouseButton::Count;
            }
        }

        static GamepadAxis mapGamepadAxis(SDL_GamepadAxis a) noexcept
        {
            switch (a)
            {
                case SDL_GAMEPAD_AXIS_LEFTX:          return GamepadAxis::LeftX;
                case SDL_GAMEPAD_AXIS_LEFTY:          return GamepadAxis::LeftY;
                case SDL_GAMEPAD_AXIS_RIGHTX:         return GamepadAxis::RightX;
                case SDL_GAMEPAD_AXIS_RIGHTY:         return GamepadAxis::RightY;
                case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:   return GamepadAxis::LeftTrigger;
                case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:  return GamepadAxis::RightTrigger;
                default:                              return GamepadAxis::Count;  // unmapped
            }
        }

        SDL3WindowManager m_windows;
        SDL3InputManager m_input;
        bool m_initialized = false;
        bool m_running = true;
    };

    // Factory the desktop entry point calls to create the shell.
    [[nodiscard]] std::unique_ptr<IShell> createShell(const WindowSettings& settings = {})
    {
        return std::make_unique<SDL3Shell>(settings);
    }
}
