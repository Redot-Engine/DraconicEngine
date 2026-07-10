// shell.null:input - no-op input devices: report nothing held/pressed so headless
// callers can use input() uniformly without null checks. Declarations only; the
// implementation lives in NullInput.cpp.
module;

#include <span>

export module shell.null:input;

import core.stdtypes;
import shell;

export namespace draco::shell
{
    class NullKeyboard final : public IKeyboard
    {
    public:
        [[nodiscard]] bool isKeyDown(KeyCode) const override;
        [[nodiscard]] bool isKeyPressed(KeyCode) const override;
        [[nodiscard]] bool isKeyReleased(KeyCode) const override;
        [[nodiscard]] KeyModifiers modifiers() const override;
    };

    class NullMouse final : public IMouse
    {
    public:
        [[nodiscard]] f32 x() const override;
        [[nodiscard]] f32 y() const override;
        [[nodiscard]] f32 deltaX() const override;
        [[nodiscard]] f32 deltaY() const override;
        [[nodiscard]] f32 scrollX() const override;
        [[nodiscard]] f32 scrollY() const override;
        [[nodiscard]] bool isButtonDown(MouseButton) const override;
        [[nodiscard]] bool isButtonPressed(MouseButton) const override;
        [[nodiscard]] bool isButtonReleased(MouseButton) const override;
        [[nodiscard]] bool relativeMode() const override;
        void setRelativeMode(bool) override;
        [[nodiscard]] bool cursorVisible() const override;
        void setCursorVisible(bool) override;
        void setCursor(CursorType) override;
    };

    class NullTouch final : public ITouch
    {
    public:
        [[nodiscard]] i32 touchCount() const override;
        [[nodiscard]] bool getTouchPoint(i32, TouchPoint&) const override;
        [[nodiscard]] bool hasTouch() const override;
    };

    class NullInputManager final : public IInputManager
    {
    public:
        [[nodiscard]] IKeyboard* keyboard() override;
        [[nodiscard]] IMouse*    mouse()    override;
        [[nodiscard]] ITouch*    touch()    override;
        [[nodiscard]] i32 gamepadCount() const override;
        [[nodiscard]] IGamepad*  getGamepad(i32) override;
        [[nodiscard]] std::span<const InputEvent> events() const override;
        [[nodiscard]] u32 hoverWindow()   const override;
        [[nodiscard]] u32 focusedWindow() const override;
        void update() override;

    private:
        NullKeyboard m_keyboard;
        NullMouse    m_mouse;
        NullTouch    m_touch;
    };
}
