// shell - the OS/windowing service (the "shell") for the engine: windows, input,
// and the per-frame event pump behind one interface. This primary module
// re-exports the window and input partitions and defines IShell, the top-level
// handle a runner drives each frame. The null and SDL3 (shell.desktop) backends
// implement it.

export module shell;

export import core.stdtypes;
export import :window;
export import :input;

export namespace draco::shell
{
    // Top-level handle to the running shell. Owns the window manager and the input
    // manager and pumps OS events. A runner creates one backend instance, then
    // each frame calls processEvents() and checks isRunning().
    class IShell
    {
    public:
        virtual ~IShell() = default;

        // The window manager (always present; owns 0..N windows). The main window
        // is windowManager()->mainWindow().
        [[nodiscard]] virtual IWindowManager* windowManager() noexcept = 0;

        // Convenience for single-window callers; equals windowManager()->mainWindow().
        // Kept so single-window hosts like the RHI sample framework stay unchanged.
        [[nodiscard]] virtual IWindow* mainWindow() noexcept = 0;

        // Aggregate input devices (keyboard/mouse/gamepad/touch). Always present;
        // a headless backend returns a no-op manager, so callers need not check.
        [[nodiscard]] virtual IInputManager* input() noexcept = 0;

        // Pump pending OS events once per frame (the runner calls this). The
        // backend rolls input state (input()->update()) before pumping.
        virtual void processEvents() = 0;

        // OS-level run state: false once the shell should quit, e.g. the main
        // window closed. Distinct from application-level exit.
        [[nodiscard]] virtual bool isRunning() const noexcept = 0;

        // Ask the shell to quit (flips isRunning() to false).
        virtual void requestExit() = 0;
    };

    // Creates the platform shell backend. Which backend is used is a compile-time
    // decision: the definition is supplied by whichever backend is linked (the SDL3
    // desktop backend, or the headless null backend). Pair every createShell() with a
    // destroyShell(); the caller owns the returned shell.
    [[nodiscard]] IShell* createShell(const WindowSettings& settings = {});

    // Destroys a shell returned by createShell().
    void destroyShell(IShell* shell) noexcept;
}
