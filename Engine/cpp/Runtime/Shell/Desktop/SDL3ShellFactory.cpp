// Supplies the shell factory bound to the SDL3 desktop backend. Linking this backend
// is what makes createShell() build an SDL3Shell, so the backend choice is a link-time
// (compile-time) decision rather than a runtime one.
module shell;

import shell.desktop;

namespace draco::shell
{
    IShell* createShell(const WindowSettings& settings)
    {
        return new SDL3Shell(settings);
    }

    void destroyShell(IShell* shell) noexcept
    {
        delete shell;
    }
}
