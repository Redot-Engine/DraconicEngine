module;

#include "impl/platform_impl.h"

export module platform;

export namespace draco::platform {

    using NativeWindowType = impl::NativeWindowType;
    using NativeWindowFrame = impl::NativeWindowFrame;

    NativeWindowFrame get_native_handles(void* sdl_window_ptr) {
        return impl::get_native_handles(sdl_window_ptr);
    }
}
