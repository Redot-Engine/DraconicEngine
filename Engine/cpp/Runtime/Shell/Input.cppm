// shell:input - input types and device interfaces for the shell module:
// keyboard, mouse, gamepad, and touch, plus the per-frame InputEvent stream and
// the IInputManager that aggregates them. Interface only; the null and SDL3
// backends implement it.

module;

#include <span>
#include <string_view>

export module shell:input;

export import core.stdtypes;

export namespace draco::shell
{
    // ---- Keyboard ---------------------------------------------------------

    enum class KeyCode : u32
    {
        Unknown = 0,
        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
        Return, Escape, Backspace, Tab, Space,
        Minus, Equals, LeftBracket, RightBracket, Backslash, Semicolon,
        Apostrophe, Grave, Comma, Period, Slash,
        CapsLock, ScrollLock, NumLock,
        PrintScreen, Pause, Insert, Home, PageUp,
        Delete, End, PageDown,
        Right, Left, Down, Up,
        Keypad0, Keypad1, Keypad2, Keypad3, Keypad4,
        Keypad5, Keypad6, Keypad7, Keypad8, Keypad9,
        KeypadDivide, KeypadMultiply, KeypadMinus, KeypadPlus,
        KeypadEnter, KeypadDecimal,
        LeftCtrl, LeftShift, LeftAlt, LeftGui,
        RightCtrl, RightShift, RightAlt, RightGui,
        Menu,

        // Number of distinct codes; backends size their key arrays from this.
        Count,
    };

    // Bitmask of the modifier keys held during a key event. Combine and test with
    // the operators below and HasFlag().
    enum class KeyModifiers : u32
    {
        None       = 0,
        LeftShift  = 1 << 0,
        RightShift = 1 << 1,
        LeftCtrl   = 1 << 2,
        RightCtrl  = 1 << 3,
        LeftAlt    = 1 << 4,
        RightAlt   = 1 << 5,
        LeftGui    = 1 << 6,
        RightGui   = 1 << 7,
        NumLock    = 1 << 8,
        CapsLock   = 1 << 9,
        ScrollLock = 1 << 10,

        // Side-agnostic combinations (either left or right).
        Shift = LeftShift | RightShift,
        Ctrl  = LeftCtrl  | RightCtrl,
        Alt   = LeftAlt   | RightAlt,
        Gui   = LeftGui   | RightGui,
    };

    constexpr KeyModifiers operator|(KeyModifiers a, KeyModifiers b) noexcept
    {
        return static_cast<KeyModifiers>(static_cast<u32>(a) | static_cast<u32>(b));
    }
    constexpr KeyModifiers operator&(KeyModifiers a, KeyModifiers b) noexcept
    {
        return static_cast<KeyModifiers>(static_cast<u32>(a) & static_cast<u32>(b));
    }
    constexpr KeyModifiers& operator|=(KeyModifiers& a, KeyModifiers b) noexcept
    {
        a = a | b;
        return a;
    }
    // True when every bit in `flag` is set in `mods`.
    constexpr bool HasFlag(KeyModifiers mods, KeyModifiers flag) noexcept
    {
        return (mods & flag) == flag;
    }

    // ---- Mouse ------------------------------------------------------------

    enum class MouseButton : u32
    {
        Left, Middle, Right, X1, X2,

        Count,
    };

    // Cursor shape passed to IMouse::setCursor.
    enum class CursorType : u32
    {
        Default, Text, Wait, Crosshair, Progress,
        ResizeNWSE, ResizeNESW, ResizeEW, ResizeNS,
        ResizeNW, ResizeN, ResizeNE, ResizeE,
        ResizeSE, ResizeS, ResizeSW, ResizeW,
        Move, NotAllowed, Pointer,

        Count,
    };

    // ---- Gamepad ----------------------------------------------------------

    enum class GamepadButton : u32
    {
        South, East, West, North,
        LeftShoulder, RightShoulder,
        LeftStick, RightStick,
        DPadUp, DPadDown, DPadLeft, DPadRight,
        Back, Guide, Start,
        LeftPaddle1, LeftPaddle2, RightPaddle1, RightPaddle2,
        Touchpad, Misc1,

        Count,
    };

    enum class GamepadAxis : u32
    {
        LeftX, LeftY, RightX, RightY,
        LeftTrigger, RightTrigger,

        Count,
    };

    // ---- Touch ------------------------------------------------------------

    // One active touch point. Positions are normalized to [0,1] across the
    // window; pressure is in [0,1].
    struct TouchPoint
    {
        u64 id       = 0;
        f32 x        = 0.0f;
        f32 y        = 0.0f;
        f32 pressure = 1.0f;
    };

    // ---- Input events (the event-first source of truth) -------------------
    //
    // Every input state change is emitted as an InputEvent, tagged with the source
    // window. The manager's polled device snapshots are a fold over the frame's
    // events, and higher layers (viewport surfaces, UI dispatch) consume the same
    // stream, so the polled and event views never disagree.

    enum class InputEventKind : u8
    {
        KeyDown, KeyUp, TextInput,
        MouseMove, MouseButtonDown, MouseButtonUp, MouseWheel,
        GamepadButtonDown, GamepadButtonUp, GamepadAxis,
        TouchDown, TouchMove, TouchUp,
    };

    // A single input event. `kind` selects which of the payload fields below carry
    // meaning; the trailing comment on each field names the kinds that use it.
    struct InputEvent
    {
        InputEventKind kind{};
        u32     window = 0;    // source window id (0 = unknown/global)

        KeyCode        key{};                          // KeyDown/KeyUp
        KeyModifiers   modifiers{};                    // KeyDown/KeyUp
        MouseButton    button{};                       // MouseButtonDown/Up
        GamepadButton  padButton{};                    // GamepadButtonDown/Up
        GamepadAxis    padAxis{};                      // GamepadAxis
        i32     gamepad = 0;                     // gamepad device index (Gamepad* kinds)
        f32     x = 0.0f, y = 0.0f;             // window-space pos (MouseMove/Touch*) or wheel delta
        f32     dx = 0.0f, dy = 0.0f;           // relative movement (MouseMove)
        f32     value = 0.0f;                    // GamepadAxis value or touch pressure
        u64     touchId = 0;                     // touch point id (Touch* kinds)
        char8_t        text[32] = {};                   // TextInput (UTF-8, null-terminated)
    };

    // ---- Device interfaces ------------------------------------------------

    // Keyboard state for the current frame. "Pressed"/"released" are edges (this
    // frame only); "down" is the held level.
    class IKeyboard
    {
    public:
        virtual ~IKeyboard() = default;

        [[nodiscard]] virtual bool isKeyDown(KeyCode key)     const = 0;  // held
        [[nodiscard]] virtual bool isKeyPressed(KeyCode key)  const = 0;  // went down this frame
        [[nodiscard]] virtual bool isKeyReleased(KeyCode key) const = 0;  // went up this frame
        [[nodiscard]] virtual KeyModifiers modifiers()        const = 0;  // modifiers currently held
    };

    // Mouse position, movement, and button state for the current frame, plus
    // cursor control.
    class IMouse
    {
    public:
        virtual ~IMouse() = default;

        [[nodiscard]] virtual f32 x()       const = 0;  // window-space position
        [[nodiscard]] virtual f32 y()       const = 0;
        [[nodiscard]] virtual f32 deltaX()  const = 0;  // movement this frame
        [[nodiscard]] virtual f32 deltaY()  const = 0;
        [[nodiscard]] virtual f32 scrollX() const = 0;  // wheel this frame
        [[nodiscard]] virtual f32 scrollY() const = 0;

        [[nodiscard]] virtual bool isButtonDown(MouseButton button)     const = 0;  // held
        [[nodiscard]] virtual bool isButtonPressed(MouseButton button)  const = 0;  // went down this frame
        [[nodiscard]] virtual bool isButtonReleased(MouseButton button) const = 0;  // went up this frame

        // Relative (locked) mouse mode: the cursor is hidden and movement is
        // reported as deltas, for camera/FPS control.
        [[nodiscard]] virtual bool relativeMode() const = 0;
        virtual void setRelativeMode(bool enabled) = 0;

        [[nodiscard]] virtual bool cursorVisible() const = 0;
        virtual void setCursorVisible(bool visible) = 0;
        virtual void setCursor(CursorType cursor) = 0;
    };

    // A connected gamepad. Buttons follow the same held/edge model as the
    // keyboard and mouse; axes are analog.
    class IGamepad
    {
    public:
        virtual ~IGamepad() = default;

        [[nodiscard]] virtual i32       index()     const = 0;  // slot in IInputManager's list
        [[nodiscard]] virtual std::u8string_view name()    const = 0;
        [[nodiscard]] virtual bool             connected() const = 0;

        [[nodiscard]] virtual bool isButtonDown(GamepadButton button)     const = 0;  // held
        [[nodiscard]] virtual bool isButtonPressed(GamepadButton button)  const = 0;  // went down this frame
        [[nodiscard]] virtual bool isButtonReleased(GamepadButton button) const = 0;  // went up this frame
        [[nodiscard]] virtual f32 axis(GamepadAxis axis)             const = 0;  // sticks [-1,1], triggers [0,1]

        // Low- and high-frequency motor strengths in [0,1], for durationMs
        // milliseconds. No-op if the gamepad has no rumble.
        virtual void setRumble(f32 lowFreq, f32 highFreq, u32 durationMs) = 0;
    };

    // Active touch points for the current frame.
    class ITouch
    {
    public:
        virtual ~ITouch() = default;

        [[nodiscard]] virtual i32 touchCount() const = 0;
        // Copy the touch point at `index` (0..touchCount()) into `out`. Returns
        // false (leaving `out` untouched) if the index is out of range.
        [[nodiscard]] virtual bool getTouchPoint(i32 index, TouchPoint& out) const = 0;
        [[nodiscard]] virtual bool hasTouch() const = 0;
    };

    // Aggregates the input devices and the frame's event stream. Exposed by
    // IShell::input(); the devices are owned by the manager.
    class IInputManager
    {
    public:
        virtual ~IInputManager() = default;

        // Always-present device accessors (a headless backend returns no-op
        // devices, so callers never need a null check).
        [[nodiscard]] virtual IKeyboard* keyboard() = 0;
        [[nodiscard]] virtual IMouse*    mouse()    = 0;
        [[nodiscard]] virtual ITouch*    touch()    = 0;

        // Connected gamepads, addressed by index (0..gamepadCount()).
        [[nodiscard]] virtual i32 gamepadCount() const = 0;
        [[nodiscard]] virtual IGamepad*  getGamepad(i32 index) = 0;

        // This frame's input events (the event-first source of truth; the device
        // snapshots above are a fold over these). Cleared each frame by update().
        [[nodiscard]] virtual std::span<const InputEvent> events() const = 0;

        // Routing authority: the window under the pointer (mouse routing) and the
        // keyboard/gamepad-focused window. 0 means none.
        [[nodiscard]] virtual u32 hoverWindow()   const = 0;
        [[nodiscard]] virtual u32 focusedWindow() const = 0;

        // Roll per-frame state (current -> previous, clear deltas and events). The
        // shell calls this once per frame before pumping OS events.
        virtual void update() = 0;
    };
}
