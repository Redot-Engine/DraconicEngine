// Headless tests for the RHI render host (GraphicsDevice + RenderWindow +
// FrameContext) over the Null RHI backend + null shell — no GPU required.
// Covers device bring-up, per-window frame begin/end, the frame-in-flight ring,
// multi-window rendering, resize, and the minimized-skip path.
#include <doctest_with_main.h>

import core;
import graphics;
import graphics.null;
import shell;
import shell.null;

using namespace draco;
using namespace draco::graphics;
using namespace draco::shell;

TEST_CASE("graphics: GraphicsDevice brings up over the null backend")
{
    auto created = createNullGraphicsDevice();
    REQUIRE(created.has_value());
    auto& gd = created.value();
    REQUIRE(gd.get() != nullptr);
    CHECK(gd->raw() != nullptr);
    CHECK(gd->gfxQueue() != nullptr);
    CHECK(gd->framesInFlight() == 2u);
    CHECK(gd->currentFrame() == 0u);
}

TEST_CASE("graphics: a window renders, and the frame ring advances")
{
    NullShell shell;
    auto created = createNullGraphicsDevice(2);
    REQUIRE(created.has_value());
    auto& gd = created.value();

    auto rwResult = gd->createRenderWindow(*shell.mainWindow(), RenderWindowDesc{});
    REQUIRE(rwResult.has_value());
    auto& rw = rwResult.value();

    // Frame 0
    FrameContext f0 = rw->beginFrame();
    CHECK(f0.valid);
    CHECK(f0.frameIndex == 0u);
    CHECK(f0.window == rw.get());
    CHECK(f0.encoder != nullptr);
    CHECK(f0.backbufferView != nullptr);
    rw->endFrame(f0);
    gd->advanceFrame();
    CHECK(gd->currentFrame() == 1u);

    // Frame 1 uses the next ring slot
    FrameContext f1 = rw->beginFrame();
    CHECK(f1.valid);
    CHECK(f1.frameIndex == 1u);
    rw->endFrame(f1);
    gd->advanceFrame();
    CHECK(gd->currentFrame() == 0u);   // wraps with framesInFlight == 2

    // Frame 2 wraps back to slot 0 and must wait the slot-0 fence cleanly.
    FrameContext f2 = rw->beginFrame();
    CHECK(f2.valid);
    CHECK(f2.frameIndex == 0u);
    rw->endFrame(f2);
}

TEST_CASE("graphics: two windows render independently in one app frame")
{
    NullShell shell;
    IWindowManager* wm = shell.windowManager();
    auto second = wm->createWindow(WindowSettings{});
    REQUIRE(second.has_value());

    auto created = createNullGraphicsDevice(2);
    REQUIRE(created.has_value());
    auto& gd = created.value();

    auto a = gd->createRenderWindow(*shell.mainWindow(), RenderWindowDesc{});
    auto b = gd->createRenderWindow(*second.value(), RenderWindowDesc{});
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());

    // Both windows render in the same app frame at the same ring index, then the
    // device advances once.
    FrameContext fa = a.value()->beginFrame();
    FrameContext fb = b.value()->beginFrame();
    CHECK(fa.valid);
    CHECK(fb.valid);
    CHECK(fa.frameIndex == fb.frameIndex);
    CHECK(fa.window != fb.window);
    a.value()->endFrame(fa);
    b.value()->endFrame(fb);
    gd->advanceFrame();
    CHECK(gd->currentFrame() == 1u);
}

TEST_CASE("graphics: syncSize resizes the swapchain when the window changes")
{
    NullShell shell;
    auto created = createNullGraphicsDevice();
    REQUIRE(created.has_value());
    auto& gd = created.value();

    auto rwResult = gd->createRenderWindow(*shell.mainWindow(), RenderWindowDesc{});
    REQUIRE(rwResult.has_value());
    auto& rw = rwResult.value();

    CHECK_FALSE(rw->syncSize());                       // nothing changed yet

    static_cast<NullWindow*>(shell.mainWindow())->resize(1600, 900);
    CHECK(rw->syncSize());                             // picked up the change
    CHECK(rw->swap()->width() == 1600u);
    CHECK(rw->swap()->height() == 900u);
    CHECK_FALSE(rw->syncSize());                       // stable again
}

TEST_CASE("graphics: a minimized window yields an invalid frame")
{
    NullShell shell;
    auto created = createNullGraphicsDevice();
    REQUIRE(created.has_value());
    auto& gd = created.value();

    auto rwResult = gd->createRenderWindow(*shell.mainWindow(), RenderWindowDesc{});
    REQUIRE(rwResult.has_value());
    auto& rw = rwResult.value();

    static_cast<NullWindow*>(shell.mainWindow())->setMinimized(true);
    FrameContext f = rw->beginFrame();
    CHECK_FALSE(f.valid);
    rw->endFrame(f);                                   // must be a harmless no-op
}
