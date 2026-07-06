// shell.null primary implementation - the headless shell bring-up.
module;

module shell.null;

namespace draco::shell
{
    NullShell::NullShell(const WindowSettings& settings) noexcept : m_windows(settings) {}

    void NullShell::processEvents() {}  // no OS event source
}
