// shell.desktop:input - the SDL3 input devices (keyboard/mouse/gamepad/touch) and the
// input manager. Declarations only; the implementation lives in Input.cpp.
module;

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

export module shell.desktop:input;

import core.stdtypes;
import shell;

export namespace draco::shell
{
    // Double-buffered device-state sizes.
    inline constexpr draco::u32 kKeyCount           = static_cast<draco::u32>(KeyCode::Count);
    inline constexpr draco::u32 kMouseButtonCount   = static_cast<draco::u32>(MouseButton::Count);
    inline constexpr draco::u32 kGamepadButtonCount = static_cast<draco::u32>(GamepadButton::Count);
    inline constexpr draco::u32 kCursorCount        = static_cast<draco::u32>(CursorType::Count);

    class SDL3Keyboard final : public IKeyboard
    {
    public:
        [[nodiscard]] bool isKeyDown(KeyCode key) const override;
        [[nodiscard]] bool isKeyPressed(KeyCode key) const override;
        [[nodiscard]] bool isKeyReleased(KeyCode key) const override;
        [[nodiscard]] KeyModifiers modifiers() const override;

        void setKey(KeyCode key, bool down);
        void setModifiers(KeyModifiers mods);
        void beginFrame();

    private:
        static draco::u32 index(KeyCode key) noexcept;
        bool m_current[kKeyCount] = {};
        bool m_previous[kKeyCount] = {};
        KeyModifiers m_mods = KeyModifiers::None;
    };

    class SDL3Mouse final : public IMouse
    {
    public:
        [[nodiscard]] draco::f32 x() const override;
        [[nodiscard]] draco::f32 y() const override;
        [[nodiscard]] draco::f32 deltaX() const override;
        [[nodiscard]] draco::f32 deltaY() const override;
        [[nodiscard]] draco::f32 scrollX() const override;
        [[nodiscard]] draco::f32 scrollY() const override;
        [[nodiscard]] bool isButtonDown(MouseButton b) const override;
        [[nodiscard]] bool isButtonPressed(MouseButton b) const override;
        [[nodiscard]] bool isButtonReleased(MouseButton b) const override;
        [[nodiscard]] bool relativeMode() const override;
        void setRelativeMode(bool enabled) override;
        [[nodiscard]] bool cursorVisible() const override;
        void setCursorVisible(bool visible) override;
        void setCursor(CursorType cursor) override;

        void setWindow(SDL_Window* window);
        // Frees the lazily-created system cursors. Called before SDL_Quit so no
        // SDL calls happen after the video subsystem is torn down.
        void releaseCursors();
        void onMotion(draco::f32 x, draco::f32 y, draco::f32 relX, draco::f32 relY);
        void onButton(draco::u32 idx, bool down);
        void onWheel(draco::f32 x, draco::f32 y);
        void beginFrame();

    private:
        static draco::u32 index(MouseButton b) noexcept;
        static SDL_SystemCursor mapSystemCursor(CursorType cursor) noexcept;

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
        SDL3Gamepad(SDL_Gamepad* pad, SDL_JoystickID id, draco::i32 index, std::u8string name) noexcept;
        // Owns the SDL_Gamepad; closing it here means the owning unique_ptr frees the
        // whole device. Must run before SDL_Quit (the shell clears the device list in
        // releaseDevices() first).
        ~SDL3Gamepad() override;

        SDL3Gamepad(const SDL3Gamepad&) = delete;
        SDL3Gamepad& operator=(const SDL3Gamepad&) = delete;

        [[nodiscard]] draco::i32 index() const override;
        [[nodiscard]] std::u8string_view name() const override;
        [[nodiscard]] bool connected() const override;
        [[nodiscard]] bool isButtonDown(GamepadButton b) const override;
        [[nodiscard]] bool isButtonPressed(GamepadButton b) const override;
        [[nodiscard]] bool isButtonReleased(GamepadButton b) const override;
        [[nodiscard]] draco::f32 axis(GamepadAxis a) const override;
        void setRumble(draco::f32 lowFreq, draco::f32 highFreq, draco::u32 durationMs) override;

        [[nodiscard]] SDL_JoystickID joystickId() const noexcept;
        [[nodiscard]] SDL_Gamepad* handle() const noexcept;
        void setIndex(draco::i32 index) noexcept;
        void setButton(GamepadButton b, bool down);
        void disconnect() noexcept;
        void beginFrame();

    private:
        static draco::u32 buttonIndex(GamepadButton b) noexcept;
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
        [[nodiscard]] draco::i32 touchCount() const override;
        [[nodiscard]] bool getTouchPoint(draco::i32 index, TouchPoint& out) const override;
        [[nodiscard]] bool hasTouch() const override;

        void addOrUpdate(const TouchPoint& tp);
        void remove(draco::u64 id);

    private:
        std::vector<TouchPoint> m_points;
    };

    class SDL3InputManager final : public IInputManager
    {
    public:
        ~SDL3InputManager() override;

        // Frees all SDL-owned input resources (open gamepads, system cursors). The
        // shell calls this before SDL_Quit; idempotent.
        void releaseDevices();

        [[nodiscard]] IKeyboard* keyboard() override;
        [[nodiscard]] IMouse*    mouse()    override;
        [[nodiscard]] ITouch*    touch()    override;
        [[nodiscard]] draco::i32 gamepadCount() const override;
        [[nodiscard]] IGamepad*  getGamepad(draco::i32 index) override;
        [[nodiscard]] std::span<const InputEvent> events() const override;
        [[nodiscard]] draco::u32 hoverWindow()   const override;
        [[nodiscard]] draco::u32 focusedWindow() const override;
        void update() override;

        // --- backend wiring (called by the shell event pump) ---
        SDL3Keyboard& keyboardDevice() noexcept;
        SDL3Mouse&    mouseDevice() noexcept;
        SDL3Touch&    touchDevice() noexcept;
        void setWindow(SDL_Window* window);
        void emitEvent(const InputEvent& e);
        void setHoverWindow(draco::u32 id) noexcept;
        void setFocusWindow(draco::u32 id) noexcept;
        void addGamepad(SDL_JoystickID id);
        void removeGamepad(SDL_JoystickID id);
        SDL3Gamepad* findGamepadById(SDL_JoystickID id);

    private:
        SDL3Keyboard m_keyboard;
        SDL3Mouse    m_mouse;
        SDL3Touch    m_touch;
        std::vector<std::unique_ptr<SDL3Gamepad>> m_gamepads;
        std::vector<InputEvent>   m_events;         // this frame's event stream
        draco::u32                m_hoverWindow = 0; // window under the pointer
        draco::u32                m_focusWindow = 0; // keyboard-focused window
    };
}
