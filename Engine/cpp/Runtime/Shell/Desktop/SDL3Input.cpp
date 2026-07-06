// shell.desktop:input implementation - SDL input-device state, cursor management, and
// the input manager. SDL is confined to this TU.
module;

#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

module shell.desktop;

namespace draco::shell
{
    // ---- SDL3Keyboard ----

    draco::u32 SDL3Keyboard::index(KeyCode key) noexcept
    {
        const draco::u32 i = static_cast<draco::u32>(key);
        return i < kKeyCount ? i : 0;
    }

    bool SDL3Keyboard::isKeyDown(KeyCode key) const { return m_current[index(key)]; }
    bool SDL3Keyboard::isKeyPressed(KeyCode key) const
    {
        const draco::u32 i = index(key);
        return m_current[i] && !m_previous[i];
    }
    bool SDL3Keyboard::isKeyReleased(KeyCode key) const
    {
        const draco::u32 i = index(key);
        return !m_current[i] && m_previous[i];
    }
    KeyModifiers SDL3Keyboard::modifiers() const { return m_mods; }
    void SDL3Keyboard::setKey(KeyCode key, bool down) { m_current[index(key)] = down; }
    void SDL3Keyboard::setModifiers(KeyModifiers mods) { m_mods = mods; }
    void SDL3Keyboard::beginFrame() { for (draco::u32 i = 0; i < kKeyCount; ++i) { m_previous[i] = m_current[i]; } }

    // ---- SDL3Mouse ----

    draco::u32 SDL3Mouse::index(MouseButton b) noexcept
    {
        const draco::u32 i = static_cast<draco::u32>(b);
        return i < kMouseButtonCount ? i : 0;
    }

    SDL_SystemCursor SDL3Mouse::mapSystemCursor(CursorType cursor) noexcept
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

    draco::f32 SDL3Mouse::x() const { return m_x; }
    draco::f32 SDL3Mouse::y() const { return m_y; }
    draco::f32 SDL3Mouse::deltaX() const { return m_dx; }
    draco::f32 SDL3Mouse::deltaY() const { return m_dy; }
    draco::f32 SDL3Mouse::scrollX() const { return m_sx; }
    draco::f32 SDL3Mouse::scrollY() const { return m_sy; }
    bool SDL3Mouse::isButtonDown(MouseButton b) const { return m_current[index(b)]; }
    bool SDL3Mouse::isButtonPressed(MouseButton b) const
    {
        const draco::u32 i = index(b);
        return m_current[i] && !m_previous[i];
    }
    bool SDL3Mouse::isButtonReleased(MouseButton b) const
    {
        const draco::u32 i = index(b);
        return !m_current[i] && m_previous[i];
    }
    bool SDL3Mouse::relativeMode() const { return m_relative; }
    void SDL3Mouse::setRelativeMode(bool enabled)
    {
        if (m_window != nullptr) { SDL_SetWindowRelativeMouseMode(m_window, enabled); }
        m_relative = enabled;
    }
    bool SDL3Mouse::cursorVisible() const { return m_cursorVisible; }
    void SDL3Mouse::setCursorVisible(bool visible)
    {
        if (visible) { SDL_ShowCursor(); } else { SDL_HideCursor(); }
        m_cursorVisible = visible;
    }
    void SDL3Mouse::setCursor(CursorType cursor)
    {
        const draco::u32 i = static_cast<draco::u32>(cursor);
        if (i >= kCursorCount) { return; }
        if (m_cursors[i] == nullptr) { m_cursors[i] = SDL_CreateSystemCursor(mapSystemCursor(cursor)); }
        if (m_cursors[i] != nullptr) { SDL_SetCursor(m_cursors[i]); m_cursor = cursor; }
    }
    void SDL3Mouse::setWindow(SDL_Window* window) { m_window = window; }
    void SDL3Mouse::releaseCursors()
    {
        for (SDL_Cursor*& c : m_cursors)
        {
            if (c != nullptr) { SDL_DestroyCursor(c); c = nullptr; }
        }
    }
    void SDL3Mouse::onMotion(draco::f32 x, draco::f32 y, draco::f32 relX, draco::f32 relY)
    {
        m_x = x; m_y = y; m_dx += relX; m_dy += relY;
    }
    void SDL3Mouse::onButton(draco::u32 idx, bool down) { if (idx < kMouseButtonCount) { m_current[idx] = down; } }
    void SDL3Mouse::onWheel(draco::f32 x, draco::f32 y) { m_sx += x; m_sy += y; }
    void SDL3Mouse::beginFrame()
    {
        for (draco::u32 i = 0; i < kMouseButtonCount; ++i) { m_previous[i] = m_current[i]; }
        m_dx = m_dy = m_sx = m_sy = 0.0f;
    }

    // ---- SDL3Gamepad ----

    draco::u32 SDL3Gamepad::buttonIndex(GamepadButton b) noexcept
    {
        const draco::u32 i = static_cast<draco::u32>(b);
        return i < kGamepadButtonCount ? i : 0;
    }

    SDL3Gamepad::SDL3Gamepad(SDL_Gamepad* pad, SDL_JoystickID id, draco::i32 index, std::u8string name) noexcept
        : m_pad(pad), m_id(id), m_index(index), m_name(std::move(name)) {}

    SDL3Gamepad::~SDL3Gamepad() { if (m_pad != nullptr) { SDL_CloseGamepad(m_pad); } }

    draco::i32 SDL3Gamepad::index() const { return m_index; }
    std::u8string_view SDL3Gamepad::name() const { return m_name; }
    bool SDL3Gamepad::connected() const { return m_pad != nullptr; }
    bool SDL3Gamepad::isButtonDown(GamepadButton b) const { return m_current[buttonIndex(b)]; }
    bool SDL3Gamepad::isButtonPressed(GamepadButton b) const
    {
        const draco::u32 i = buttonIndex(b);
        return m_current[i] && !m_previous[i];
    }
    bool SDL3Gamepad::isButtonReleased(GamepadButton b) const
    {
        const draco::u32 i = buttonIndex(b);
        return !m_current[i] && m_previous[i];
    }
    draco::f32 SDL3Gamepad::axis(GamepadAxis a) const
    {
        if (m_pad == nullptr) { return 0.0f; }
        const auto raw = SDL_GetGamepadAxis(m_pad, static_cast<SDL_GamepadAxis>(static_cast<draco::u32>(a)));
        return static_cast<draco::f32>(raw) / 32767.0f;
    }
    void SDL3Gamepad::setRumble(draco::f32 lowFreq, draco::f32 highFreq, draco::u32 durationMs)
    {
        if (m_pad != nullptr)
        {
            SDL_RumbleGamepad(m_pad,
                              static_cast<draco::u16>(lowFreq * 65535.0f),
                              static_cast<draco::u16>(highFreq * 65535.0f),
                              durationMs);
        }
    }
    SDL_JoystickID SDL3Gamepad::joystickId() const noexcept { return m_id; }
    SDL_Gamepad* SDL3Gamepad::handle() const noexcept { return m_pad; }
    void SDL3Gamepad::setIndex(draco::i32 index) noexcept { m_index = index; }
    void SDL3Gamepad::setButton(GamepadButton b, bool down) { m_current[buttonIndex(b)] = down; }
    void SDL3Gamepad::disconnect() noexcept { m_pad = nullptr; }
    void SDL3Gamepad::beginFrame() { for (draco::u32 i = 0; i < kGamepadButtonCount; ++i) { m_previous[i] = m_current[i]; } }

    // ---- SDL3Touch ----

    draco::i32 SDL3Touch::touchCount() const { return static_cast<draco::i32>(m_points.size()); }
    bool SDL3Touch::getTouchPoint(draco::i32 index, TouchPoint& out) const
    {
        if (index < 0 || static_cast<draco::usize>(index) >= m_points.size()) { return false; }
        out = m_points[static_cast<draco::usize>(index)];
        return true;
    }
    bool SDL3Touch::hasTouch() const { return !m_points.empty(); }
    void SDL3Touch::addOrUpdate(const TouchPoint& tp)
    {
        for (draco::usize i = 0; i < m_points.size(); ++i)
        {
            if (m_points[i].id == tp.id) { m_points[i] = tp; return; }
        }
        m_points.push_back(tp);
    }
    void SDL3Touch::remove(draco::u64 id)
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

    // ---- SDL3InputManager ----

    SDL3InputManager::~SDL3InputManager() { releaseDevices(); }

    void SDL3InputManager::releaseDevices()
    {
        m_gamepads.clear();   // each SDL3Gamepad dtor closes its SDL_Gamepad
        m_mouse.releaseCursors();
    }

    IKeyboard* SDL3InputManager::keyboard() { return &m_keyboard; }
    IMouse*    SDL3InputManager::mouse()    { return &m_mouse; }
    ITouch*    SDL3InputManager::touch()    { return &m_touch; }
    draco::i32 SDL3InputManager::gamepadCount() const { return static_cast<draco::i32>(m_gamepads.size()); }
    IGamepad*  SDL3InputManager::getGamepad(draco::i32 index)
    {
        if (index < 0 || static_cast<draco::usize>(index) >= m_gamepads.size()) { return nullptr; }
        return m_gamepads[static_cast<draco::usize>(index)].get();
    }
    std::span<const InputEvent> SDL3InputManager::events() const
    {
        return std::span<const InputEvent>{ m_events.data(), m_events.size() };
    }
    draco::u32 SDL3InputManager::hoverWindow()   const { return m_hoverWindow; }
    draco::u32 SDL3InputManager::focusedWindow() const { return m_focusWindow; }
    void SDL3InputManager::update()
    {
        m_keyboard.beginFrame();
        m_mouse.beginFrame();
        for (auto& g : m_gamepads) { g->beginFrame(); }
        m_events.clear();   // events are valid only for the frame they were pumped in
    }

    SDL3Keyboard& SDL3InputManager::keyboardDevice() noexcept { return m_keyboard; }
    SDL3Mouse&    SDL3InputManager::mouseDevice() noexcept { return m_mouse; }
    SDL3Touch&    SDL3InputManager::touchDevice() noexcept { return m_touch; }
    void SDL3InputManager::setWindow(SDL_Window* window) { m_mouse.setWindow(window); }
    void SDL3InputManager::emitEvent(const InputEvent& e) { m_events.push_back(e); }
    void SDL3InputManager::setHoverWindow(draco::u32 id) noexcept { m_hoverWindow = id; }
    void SDL3InputManager::setFocusWindow(draco::u32 id) noexcept { m_focusWindow = id; }

    void SDL3InputManager::addGamepad(SDL_JoystickID id)
    {
        if (findGamepadById(id) != nullptr) { return; }
        SDL_Gamepad* pad = SDL_OpenGamepad(id);
        if (pad == nullptr) { return; }

        const char* nm = SDL_GetGamepadName(pad);
        std::u8string name = (nm != nullptr)
            ? std::u8string(reinterpret_cast<const char8_t*>(nm))
            : std::u8string{};
        const draco::i32 index = static_cast<draco::i32>(m_gamepads.size());
        m_gamepads.push_back(std::make_unique<SDL3Gamepad>(pad, id, index, std::move(name)));
    }

    void SDL3InputManager::removeGamepad(SDL_JoystickID id)
    {
        for (draco::usize i = 0; i < m_gamepads.size(); ++i)
        {
            if (m_gamepads[i]->joystickId() == id)
            {
                m_gamepads.erase(m_gamepads.begin() + static_cast<std::ptrdiff_t>(i));  // dtor closes the SDL_Gamepad
                for (draco::usize j = 0; j < m_gamepads.size(); ++j) { m_gamepads[j]->setIndex(static_cast<draco::i32>(j)); }
                return;
            }
        }
    }

    SDL3Gamepad* SDL3InputManager::findGamepadById(SDL_JoystickID id)
    {
        for (auto& g : m_gamepads) { if (g->joystickId() == id) { return g.get(); } }
        return nullptr;
    }
}
