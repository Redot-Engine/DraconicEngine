// shell.null:input implementation - no-op input devices.
module;

#include <span>

module shell.null;

namespace draco::shell
{
    // ---- NullKeyboard ----

    bool NullKeyboard::isKeyDown(KeyCode) const { return false; }
    bool NullKeyboard::isKeyPressed(KeyCode) const { return false; }
    bool NullKeyboard::isKeyReleased(KeyCode) const { return false; }
    KeyModifiers NullKeyboard::modifiers() const { return KeyModifiers::None; }

    // ---- NullMouse ----

    f32 NullMouse::x() const { return 0.0f; }
    f32 NullMouse::y() const { return 0.0f; }
    f32 NullMouse::deltaX() const { return 0.0f; }
    f32 NullMouse::deltaY() const { return 0.0f; }
    f32 NullMouse::scrollX() const { return 0.0f; }
    f32 NullMouse::scrollY() const { return 0.0f; }
    bool NullMouse::isButtonDown(MouseButton) const { return false; }
    bool NullMouse::isButtonPressed(MouseButton) const { return false; }
    bool NullMouse::isButtonReleased(MouseButton) const { return false; }
    bool NullMouse::relativeMode() const { return false; }
    void NullMouse::setRelativeMode(bool) {}
    bool NullMouse::cursorVisible() const { return true; }
    void NullMouse::setCursorVisible(bool) {}
    void NullMouse::setCursor(CursorType) {}

    // ---- NullTouch ----

    i32 NullTouch::touchCount() const { return 0; }
    bool NullTouch::getTouchPoint(i32, TouchPoint&) const { return false; }
    bool NullTouch::hasTouch() const { return false; }

    // ---- NullInputManager ----

    IKeyboard* NullInputManager::keyboard() { return &m_keyboard; }
    IMouse*    NullInputManager::mouse()    { return &m_mouse; }
    ITouch*    NullInputManager::touch()    { return &m_touch; }
    i32 NullInputManager::gamepadCount() const { return 0; }
    IGamepad*  NullInputManager::getGamepad(i32) { return nullptr; }
    std::span<const InputEvent> NullInputManager::events() const { return {}; }
    u32 NullInputManager::hoverWindow()   const { return 0; }
    u32 NullInputManager::focusedWindow() const { return 0; }
    void NullInputManager::update() {}
}
