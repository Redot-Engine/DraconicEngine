# Platform-specific source selection
if(WIN32)
    set(PLATFORM_SRC "win32/win32.cpp")
    set(PLATFORM_DEPS "") 
elseif(APPLE)
    set(PLATFORM_SRC "mac/mac.mm")
    set(PLATFORM_DEPS "-framework Cocoa -framework Metal -framework QuartzCore")
else()
    set(PLATFORM_SRC "linux/linux.cpp")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(X11_LIBS REQUIRED IMPORTED_TARGET x11 xext xcursor xrandr xrender xi xfixes)
    pkg_check_modules(WAYLAND_LIBS IMPORTED_TARGET wayland-client wayland-egl wayland-cursor egl)
    set(PLATFORM_DEPS PkgConfig::X11_LIBS)
    if(WAYLAND_LIBS_FOUND)
        list(APPEND PLATFORM_DEPS PkgConfig::WAYLAND_LIBS)
    endif()
endif()

add_library(platform_impl STATIC
    ${PLATFORM_SRC}
)
target_include_directories(platform_impl
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)
target_link_libraries(platform_impl PUBLIC SDL3::SDL3-static PRIVATE ${PLATFORM_DEPS})
if(DRACO_HAS_WAYLAND)
    target_compile_definitions(platform_impl PRIVATE DRACO_HAS_WAYLAND=1)
endif()