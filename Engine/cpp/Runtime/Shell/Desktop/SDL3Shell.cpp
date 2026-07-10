// Desktop shell (SDL3) implementation: the SDL_Init/window bring-up, the event pump,
// and the SDL/engine input mappings - the bodies too large to inline in the module
// interface. SDL is confined to this translation unit.
module;

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>   // SDL_SetMainReady

#include <expected>

module shell.desktop;

namespace draco::shell
{
    namespace
    {
    KeyCode mapKeyCode(SDL_Scancode sc) noexcept
    {
        if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
            return static_cast<KeyCode>(static_cast<u32>(KeyCode::A) + (sc - SDL_SCANCODE_A));
        if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
            return static_cast<KeyCode>(static_cast<u32>(KeyCode::Num1) + (sc - SDL_SCANCODE_1));
        if (sc == SDL_SCANCODE_0) return KeyCode::Num0;
        if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12)
            return static_cast<KeyCode>(static_cast<u32>(KeyCode::F1) + (sc - SDL_SCANCODE_F1));
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

    KeyModifiers mapModifiers(SDL_Keymod mod) noexcept
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
    GamepadButton mapGamepadButton(SDL_GamepadButton b) noexcept
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
    MouseButton mapMouseButton(u32 idx) noexcept
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

    GamepadAxis mapGamepadAxis(SDL_GamepadAxis a) noexcept
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
    }

    SDL3Shell::SDL3Shell(const WindowSettings& settings) noexcept
    {
        SDL_SetMainReady();
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { m_running = false; return; }
        m_initialized = true;

        std::expected<IWindow*, WindowError> main = m_windows.createWindow(settings);
        if (!main.has_value()) { m_running = false; return; }
        if (SDL3Window* w = m_windows.find(main.value()->id())) { m_input.setWindow(static_cast<SDL_Window*>(w->handle())); }
    }

    SDL3Shell::~SDL3Shell()
    {
        // Release SDL-owned input resources and destroy windows before tearing SDL
        // down (no SDL calls may happen after SDL_Quit).
        m_input.releaseDevices();
        m_windows.destroyAllNow();
        if (m_initialized) { SDL_Quit(); }
    }

    void SDL3Shell::processEvents()
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
                    const u32 id = static_cast<u32>(event.window.windowID);
                    m_windows.pushEvent(WindowEvent{ WindowEventType::CloseRequested, id });
                    // Closing the main window stops the shell; the runner
                    // handles secondary-window close via the event queue.
                    IWindow* main = m_windows.mainWindow();
                    if (main != nullptr && main->id() == id) { main->close(); m_running = false; }
                    break;
                }
                case SDL_EVENT_WINDOW_RESIZED:
                {
                    const u32 id = static_cast<u32>(event.window.windowID);
                    if (SDL3Window* w = m_windows.find(id))
                    {
                        const u32 nw = static_cast<u32>(event.window.data1);
                        const u32 nh = static_cast<u32>(event.window.data2);
                        w->onResized(nw, nh);
                        m_windows.pushEvent(WindowEvent{ WindowEventType::Resized, id, nw, nh });
                    }
                    break;
                }
                case SDL_EVENT_WINDOW_FOCUS_GAINED:
                {
                    const u32 id = static_cast<u32>(event.window.windowID);
                    m_input.setFocusWindow(id);   // keyboard/gamepad routing authority
                    m_windows.pushEvent(WindowEvent{ WindowEventType::FocusGained, id });
                    break;
                }
                case SDL_EVENT_WINDOW_FOCUS_LOST:
                {
                    const u32 id = static_cast<u32>(event.window.windowID);
                    if (m_input.focusedWindow() == id) { m_input.setFocusWindow(0); }
                    m_windows.pushEvent(WindowEvent{ WindowEventType::FocusLost, id });
                    break;
                }
                case SDL_EVENT_WINDOW_MOUSE_ENTER:
                    m_input.setHoverWindow(static_cast<u32>(event.window.windowID));
                    break;
                case SDL_EVENT_WINDOW_MOUSE_LEAVE:
                    if (m_input.hoverWindow() == static_cast<u32>(event.window.windowID))
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
                    e.window = static_cast<u32>(event.key.windowID);
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
                    e.window = static_cast<u32>(event.text.windowID);
                    if (event.text.text != nullptr)
                    {
                        usize n = 0;
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
                    m_input.setHoverWindow(static_cast<u32>(event.motion.windowID));
                    InputEvent e{};
                    e.kind = InputEventKind::MouseMove;
                    e.window = static_cast<u32>(event.motion.windowID);
                    e.x = event.motion.x; e.y = event.motion.y;
                    e.dx = event.motion.xrel; e.dy = event.motion.yrel;
                    m_input.emitEvent(e);
                    m_input.mouseDevice().onMotion(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
                    break;
                }
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                {
                    const u32 btn = static_cast<u32>(event.button.button) - 1;
                    const bool down = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                    InputEvent e{};
                    e.kind = down ? InputEventKind::MouseButtonDown : InputEventKind::MouseButtonUp;
                    e.window = static_cast<u32>(event.button.windowID);
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
                    e.window = static_cast<u32>(event.wheel.windowID);
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
                    e.window = static_cast<u32>(event.tfinger.windowID);
                    e.touchId = static_cast<u64>(event.tfinger.fingerID);
                    e.x = event.tfinger.x; e.y = event.tfinger.y; e.value = event.tfinger.pressure;
                    m_input.emitEvent(e);
                    m_input.touchDevice().addOrUpdate(TouchPoint{ e.touchId, e.x, e.y, e.value });
                    break;
                }
                case SDL_EVENT_FINGER_UP:
                {
                    InputEvent e{};
                    e.kind = InputEventKind::TouchUp;
                    e.window = static_cast<u32>(event.tfinger.windowID);
                    e.touchId = static_cast<u64>(event.tfinger.fingerID);
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
                            e.value = static_cast<f32>(event.gaxis.value) / 32767.0f;   // snapshot reads axes live
                            m_input.emitEvent(e);
                        }
                    }
                    break;

                default:
                    break;
            }
        }
    }
}
