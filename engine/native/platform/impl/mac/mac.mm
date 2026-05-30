#include "platform_impl.h"

#include <SDL3/SDL.h>

namespace draco::platform::impl {
    NativeWindowFrame get_native_handles(void* sdl_window_ptr) {
        SDL_Window* window = static_cast<SDL_Window*>(sdl_window_ptr);
        NativeWindowFrame frame;
        
        SDL_PropertiesID props = SDL_GetWindowProperties(window);

        frame.nwh = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_COCOA_VIEW_POINTER, nullptr);
        frame.ndt = nullptr; 
        frame.type = NativeWindowType::Cocoa;

        SDL_GetWindowSize(window, &frame.width, &frame.height);
        frame.valid = (frame.nwh != nullptr);
        return frame;
    }
}
