// Supplies the shell factory bound to the headless null backend. Linked in place of the
// SDL3 backend (e.g. for tests / servers), which makes createShell() build a NullShell.
module shell;

import shell.null;

namespace draco::shell
{
    IShell* createShell(const WindowSettings& settings)
    {
        return new NullShell(settings);
    }

    void destroyShell(IShell* shell) noexcept
    {
        delete shell;
    }
}
