#
# Render Sample - shell-only placeholder (no renderer yet)
#
add_executable(RenderSample RenderSample.cpp)

# The single Shell library owns windowing + input; its SDL3 dependency is PRIVATE, so
# force-load SDL below to satisfy the backend at final link.
target_link_libraries(RenderSample PRIVATE Shell)

# SDL is statically linked via Shell; force-load the whole archive so its
# constructor-registered video/input drivers are not stripped by the linker.
if(UNIX AND NOT APPLE)
    target_link_libraries(RenderSample PRIVATE Threads::Threads dl)
    target_link_options(RenderSample PRIVATE -Wl,--whole-archive $<TARGET_FILE:SDL3::SDL3-static> -Wl,--no-whole-archive)
elseif(APPLE)
    target_link_options(RenderSample PRIVATE -Wl,-force_load,$<TARGET_FILE:SDL3::SDL3-static>)
elseif(MSVC)
    target_link_options(RenderSample PRIVATE /WHOLEARCHIVE:$<TARGET_FILE:SDL3::SDL3-static>)
endif()
