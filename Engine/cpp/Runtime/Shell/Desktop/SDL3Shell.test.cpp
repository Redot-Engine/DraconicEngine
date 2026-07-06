#include <doctest_with_main.h>

#include <cstdlib>           // setenv / _putenv_s
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>        // synthetic events + window id (tests exercise the close path)

import core.stdtypes;
import shell;
import shell.desktop;

using namespace draco;
using namespace draco::shell;

namespace
{
    // Force SDL's headless "dummy" video driver so these run in CI without a
    // display and never flash a real window. Set before any SDL_Init.
    struct ForceDummyDriver
    {
        ForceDummyDriver() {
#ifdef _WIN32
            _putenv_s("SDL_VIDEODRIVER", "dummy");
#else
            ::setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
        }
    };
    const ForceDummyDriver g_forceDummy;
}

TEST_CASE("shell.desktop: SDL3 shell creates a window and reports state")
{
    WindowSettings settings;
    settings.title = u8"Draconic Test";
    settings.width = 640;
    settings.height = 480;

    SDL3Shell shell(settings);
    if (shell.mainWindow() == nullptr)
    {
        MESSAGE("SDL video init/window creation unavailable; skipping");
        CHECK_FALSE(shell.isRunning());
        return;
    }

    CHECK(shell.mainWindow()->width() == 640u);
    CHECK(shell.mainWindow()->height() == 480u);
    CHECK(shell.isRunning());

    // Under the dummy driver the reported window system is shell-dependent:
    // Linux reports Unknown (no real display), Windows still reports Win32.
    // native() must be callable and self-consistent either way.
    const NativeWindow native = shell.mainWindow()->native();
#if defined(_WIN32)
    CHECK(native.system == WindowSystem::Win32);
#else
    CHECK(native.system == WindowSystem::Unknown);
#endif

    shell.processEvents();   // pump (no pending events) - must not change state
    CHECK(shell.isRunning());
}

TEST_CASE("shell.desktop: a window-close event stops the shell")
{
    SDL3Shell shell;
    if (shell.mainWindow() == nullptr) { return; }

    auto* window = static_cast<SDL_Window*>(static_cast<SDL3Window*>(shell.mainWindow())->handle());
    REQUIRE(window != nullptr);

    SDL_Event event{};
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    event.window.windowID = SDL_GetWindowID(window);
    SDL_PushEvent(&event);

    shell.processEvents();
    CHECK_FALSE(shell.isRunning());
    CHECK_FALSE(shell.mainWindow()->isOpen());
}

TEST_CASE("shell.desktop: keyboard events drive double-buffered key state")
{
    SDL3Shell shell;
    if (shell.mainWindow() == nullptr) { return; }

    IInputManager* input = shell.input();
    REQUIRE(input != nullptr);
    REQUIRE(input->keyboard() != nullptr);
    IKeyboard* kb = input->keyboard();

    const SDL_WindowID winId = SDL_GetWindowID(static_cast<SDL_Window*>(static_cast<SDL3Window*>(shell.mainWindow())->handle()));

    auto pushKey = [winId](bool down) {
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.windowID = winId;
        e.key.scancode = SDL_SCANCODE_A;
        e.key.down = down;
        e.key.mod = SDL_KMOD_LSHIFT;
        SDL_PushEvent(&e);
    };

    // Frame 1: key goes down -> Down and Pressed this frame.
    pushKey(true);
    shell.processEvents();
    CHECK(kb->isKeyDown(KeyCode::A));
    CHECK(kb->isKeyPressed(KeyCode::A));
    CHECK(HasFlag(kb->modifiers(), KeyModifiers::LeftShift));

    // Frame 2: still held, no longer "pressed this frame".
    shell.processEvents();
    CHECK(kb->isKeyDown(KeyCode::A));
    CHECK_FALSE(kb->isKeyPressed(KeyCode::A));

    // Frame 3: key goes up -> Released this frame, no longer down.
    pushKey(false);
    shell.processEvents();
    CHECK_FALSE(kb->isKeyDown(KeyCode::A));
    CHECK(kb->isKeyReleased(KeyCode::A));
}

TEST_CASE("shell.desktop: mouse motion and buttons are tracked")
{
    SDL3Shell shell;
    if (shell.mainWindow() == nullptr) { return; }

    IInputManager* input = shell.input();
    REQUIRE(input != nullptr);
    IMouse* mouse = input->mouse();
    REQUIRE(mouse != nullptr);

    const SDL_WindowID winId = SDL_GetWindowID(static_cast<SDL_Window*>(static_cast<SDL3Window*>(shell.mainWindow())->handle()));

    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.windowID = winId;
    motion.motion.x = 12.0f; motion.motion.y = 34.0f;
    motion.motion.xrel = 12.0f; motion.motion.yrel = 34.0f;
    SDL_PushEvent(&motion);

    SDL_Event button{};
    button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    button.button.windowID = winId;
    button.button.button = SDL_BUTTON_LEFT;  // 1-based; maps to MouseButton::Left
    button.button.down = true;
    SDL_PushEvent(&button);

    shell.processEvents();
    CHECK(mouse->x() == 12.0f);
    CHECK(mouse->y() == 34.0f);
    CHECK(mouse->deltaX() == 12.0f);
    CHECK(mouse->isButtonDown(MouseButton::Left));
    CHECK(mouse->isButtonPressed(MouseButton::Left));

    // Next frame with no events: deltas reset, button still held.
    shell.processEvents();
    CHECK(mouse->deltaX() == 0.0f);
    CHECK(mouse->isButtonDown(MouseButton::Left));
    CHECK_FALSE(mouse->isButtonPressed(MouseButton::Left));
}

TEST_CASE("shell.desktop: cursor state is settable")
{
    SDL3Shell shell;
    if (shell.mainWindow() == nullptr) { return; }

    IMouse* mouse = shell.input()->mouse();
    REQUIRE(mouse != nullptr);

    CHECK(mouse->cursorVisible());
    mouse->setCursorVisible(false);
    CHECK_FALSE(mouse->cursorVisible());
    mouse->setCursorVisible(true);
    CHECK(mouse->cursorVisible());

    // Exercises the system-cursor cache/mapping across a range of types; under
    // the dummy driver creation may fail, but the calls must be safe either way.
    mouse->setCursor(CursorType::Pointer);
    mouse->setCursor(CursorType::Text);
    mouse->setCursor(CursorType::ResizeNWSE);
    mouse->setCursor(CursorType::Pointer);  // cached on second use
    mouse->setCursor(CursorType::Default);
}

TEST_CASE("shell.desktop: input exposes a gamepad list")
{
    SDL3Shell shell;
    if (shell.mainWindow() == nullptr) { return; }

    IInputManager* input = shell.input();
    REQUIRE(input != nullptr);
    // No physical gamepads under the dummy driver; the list is simply empty.
    CHECK(input->gamepadCount() == 0);
    CHECK(input->getGamepad(0) == nullptr);
}

TEST_CASE("shell.desktop: requestExit stops the shell")
{
    SDL3Shell shell;
    if (shell.mainWindow() == nullptr) { return; }
    CHECK(shell.isRunning());
    shell.requestExit();
    CHECK_FALSE(shell.isRunning());
}
