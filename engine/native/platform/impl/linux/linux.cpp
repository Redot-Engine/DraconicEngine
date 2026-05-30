#include "platform_impl.h"

#include <print>
#include <string_view>

#include <SDL3/SDL.h>

#include <cstdint>

namespace draco::platform::impl {
    NativeWindowFrame get_native_handles(void* sdl_window_ptr) {
        SDL_Window* window = static_cast<SDL_Window*>(sdl_window_ptr);
        NativeWindowFrame frame;
        
        const char* driver = SDL_GetCurrentVideoDriver();
        SDL_PropertiesID props = SDL_GetWindowProperties(window);

        if (std::string_view(driver) == "x11") {
            frame.ndt = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
            frame.nwh = (void*)(std::uintptr_t)SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
            frame.type = NativeWindowType::X11;
        } else if (std::string_view(driver) == "wayland") {
            frame.ndt = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
            frame.nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
            frame.type = NativeWindowType::Wayland;
        } else {
            std::println("No video driver was found");
            SDL_Quit();
            frame.type = NativeWindowType::None;
        }
        
        std::println("Video driver: {}", driver);
        
        SDL_GetWindowSize(window, &frame.width, &frame.height);
        frame.valid = (frame.nwh != nullptr);
        return frame;
    }
}
