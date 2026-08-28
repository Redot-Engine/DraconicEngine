#include <print>
#include <chrono>
#include <thread>

import shell;
import shell.desktop;

using namespace draco::shell;

int main(int, char*[])
{
    auto shell = createShell(WindowSettings{
        .title = u8"Draconic Engine Sample",
        .width = 1280,
        .height = 720,
    });

    if (shell->mainWindow() == nullptr)
    {
        std::println("Failed to create shell window");
        return -1;
    }

    // No renderer yet: pump the shell each frame until the window is closed.
    while (shell->isRunning())
    {
        shell->processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}
