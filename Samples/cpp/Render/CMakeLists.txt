#
# Rendering Sample
#
add_executable(RenderSample RenderSample.cpp)

# The SDL3 desktop shell backend owns windowing + input; it pulls in SDL3 (PUBLIC).
target_link_libraries(RenderSample
        PRIVATE
        Runtime
        bgfx
        bx
        bimg
)

# SDL is statically linked via Shell_Desktop; force-load the whole archive so its
# constructor-registered video/input drivers are not stripped by the linker.
if(UNIX AND NOT APPLE)
    target_link_libraries(RenderSample PRIVATE Threads::Threads dl)
    target_link_options(RenderSample PRIVATE -Wl,--whole-archive $<TARGET_FILE:SDL3::SDL3-static> -Wl,--no-whole-archive)
elseif(APPLE)
    target_link_options(RenderSample PRIVATE -Wl,-force_load,$<TARGET_FILE:SDL3::SDL3-static>)
elseif(MSVC)
    target_link_options(RenderSample PRIVATE /WHOLEARCHIVE:$<TARGET_FILE:SDL3::SDL3-static>)
endif()

compile_shaders(RenderSample)

set(TEST_IMAGE
        "${CMAKE_SOURCE_DIR}/Docs/assets/draconic_logo_no_text.png"
)

add_custom_command(
        TARGET RenderSample POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${TEST_IMAGE}
        $<TARGET_FILE_DIR:RenderSample>/test.png

        COMMENT "Copying test texture..."
)
