#pragma once

namespace draco::platform::impl {

    enum class NativeWindowType
    {
        None,
        Default,
        Win32,
        Wayland,
        X11,
        Cocoa
    };

    struct NativeWindowFrame
    {
        void* nwh = nullptr; // Native Window Handle
        void* ndt = nullptr; // Native Display Type

        int width = 0;
        int height = 0;

        bool valid = false;

        NativeWindowType type = NativeWindowType::None; // Track the type of the native window
    };

    NativeWindowFrame get_native_handles(void* sdl_window_ptr);
}