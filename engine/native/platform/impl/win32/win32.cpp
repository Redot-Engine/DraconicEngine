#include "platform_impl.h"

#include <SDL3/SDL.h>

namespace draco::platform::impl {
    NativeWindowFrame get_native_handles(void* sdl_window_ptr) {
        SDL_Window* window = static_cast<SDL_Window*>(sdl_window_ptr);
        NativeWindowFrame frame;
        
        SDL_PropertiesID props = SDL_GetWindowProperties(window);

        frame.nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        frame.ndt = nullptr; // Note: On Windows, ndt isn't required by bgfx for the Vulkan & DX backends which is why we leave it as a nullptr
        frame.type = NativeWindowType::Win32;

        SDL_GetWindowSize(window, &frame.width, &frame.height);
        frame.valid = (frame.nwh != nullptr);
        return frame;
    }
}
