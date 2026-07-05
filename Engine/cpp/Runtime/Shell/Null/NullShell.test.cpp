#include <doctest_with_main.h>

import core.stdtypes;
import shell;
import shell.null;

using namespace draco;
using namespace draconic::shell;

TEST_CASE("shell.null: a headless shell reports a window and run state")
{
    WindowSettings settings;
    settings.width = 800;
    settings.height = 600;

    NullShell shell(settings);
    REQUIRE(shell.mainWindow() != nullptr);
    CHECK(shell.mainWindow()->width() == 800u);
    CHECK(shell.mainWindow()->height() == 600u);
    CHECK(shell.mainWindow()->native().system == WindowSystem::Unknown);  // headless: no handles
    CHECK(shell.mainWindow()->native().window == nullptr);
    CHECK(shell.isRunning());

    shell.processEvents();  // no-op, must not change run state
    CHECK(shell.isRunning());

    shell.requestExit();
    CHECK_FALSE(shell.isRunning());
}

TEST_CASE("shell.null: closing the window stops the shell")
{
    NullShell shell;
    CHECK(shell.isRunning());
    shell.mainWindow()->close();
    CHECK_FALSE(shell.mainWindow()->isOpen());
    CHECK_FALSE(shell.isRunning());
}

TEST_CASE("shell.null: window manager creates, lists, and looks up windows")
{
    NullShell shell;
    IWindowManager* wm = shell.windowManager();
    REQUIRE(wm != nullptr);

    // The shell seeds one main window; it is windows()[0] and mainWindow().
    REQUIRE(wm->windows().size() == 1u);
    IWindow* main = wm->mainWindow();
    REQUIRE(main != nullptr);
    CHECK(wm->windows()[0] == main);
    CHECK(main->id() != 0u);                       // 0 is never a valid id
    CHECK(wm->getWindow(main->id()) == main);
    CHECK(wm->getWindow(99999u) == nullptr);

    // Open a second window; ids are distinct, main is unchanged.
    WindowSettings s; s.width = 320; s.height = 240;
    auto second = wm->createWindow(s);
    REQUIRE(second.has_value());
    CHECK(second.value()->id() != main->id());
    CHECK(wm->windows().size() == 2u);
    CHECK(wm->mainWindow() == main);               // still the first
    CHECK(second.value()->width() == 320u);
}

TEST_CASE("shell.null: destroyWindow defers until flushDestroyed")
{
    NullShell shell;
    IWindowManager* wm = shell.windowManager();
    auto second = wm->createWindow(WindowSettings{});
    REQUIRE(second.has_value());
    const u32 secondId = second.value()->id();
    REQUIRE(wm->windows().size() == 2u);

    // Destroy is deferred: the window stays listed (but closed) until flush.
    wm->destroyWindow(second.value());
    CHECK(wm->windows().size() == 2u);
    CHECK_FALSE(second.value()->isOpen());

    wm->flushDestroyed();
    CHECK(wm->windows().size() == 1u);
    CHECK(wm->getWindow(secondId) == nullptr);
    CHECK(wm->mainWindow() != nullptr);            // main survived
}

TEST_CASE("shell.null: window events queue is empty (no OS source)")
{
    NullShell shell;
    shell.processEvents();
    CHECK(shell.windowManager()->events().size() == 0u);
}

TEST_CASE("shell.null: NullWindow resize hook updates reported size")
{
    NullShell shell;
    auto* main = static_cast<NullWindow*>(shell.mainWindow());
    main->resize(1024, 768);
    CHECK(main->width() == 1024u);
    CHECK(main->height() == 768u);
    main->setMinimized(true);
    CHECK(main->isMinimized());
}

TEST_CASE("shell.null: input is present and reports no activity")
{
    NullShell shell;
    IInputManager* input = shell.input();
    REQUIRE(input != nullptr);

    // Devices are reachable so callers need no null checks.
    REQUIRE(input->keyboard() != nullptr);
    REQUIRE(input->mouse() != nullptr);
    REQUIRE(input->touch() != nullptr);

    CHECK_FALSE(input->keyboard()->isKeyDown(KeyCode::Space));
    CHECK(input->keyboard()->modifiers() == KeyModifiers::None);
    CHECK(input->mouse()->x() == 0.0f);
    CHECK_FALSE(input->mouse()->isButtonDown(MouseButton::Left));
    CHECK(input->mouse()->cursorVisible());
    CHECK_FALSE(input->touch()->hasTouch());
    CHECK(input->gamepadCount() == 0);
    CHECK(input->getGamepad(0) == nullptr);

    input->update();  // must be a harmless no-op
}
