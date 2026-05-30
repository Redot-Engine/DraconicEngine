cmake_minimum_required(VERSION 4.0)

project(DraconicEngine LANGUAGES C CXX)

find_package(Git QUIET)
find_package(Threads REQUIRED)

if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    message(STATUS "Updating submodules...")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE GIT_SUBMOD_RESULT
    )
    
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "Failed to update submodules. Please run manually.")
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(CTest)
include(Compiler)
include(Modules)
include(Shaders)

# Subdirectories for engine libs
add_subdirectory(engine)

# Main Executable
add_executable(draconic engine/native/main/main.cpp)

target_link_libraries(draconic
    PRIVATE
    native
    bgfx
    bx
    bimg
)

if(UNIX AND NOT APPLE)
    target_link_libraries(draconic PRIVATE SDL3::SDL3-static Threads::Threads dl)
    target_link_options(draconic PRIVATE -Wl,--whole-archive $<TARGET_FILE:SDL3::SDL3-static> -Wl,--no-whole-archive)
elseif(APPLE)
    target_link_libraries(draconic PRIVATE SDL3::SDL3-static)
    target_link_options(draconic PRIVATE -Wl,-force_load,$<TARGET_FILE:SDL3::SDL3-static>)
else()
    target_link_libraries(draconic PRIVATE SDL3::SDL3-static)
endif()

compile_shaders(draconic)

set(TEST_IMAGE
    "${CMAKE_SOURCE_DIR}/assets/draconic_logo_no_text.png"
)

add_custom_command(
    TARGET draconic POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${TEST_IMAGE}
        $<TARGET_FILE_DIR:draconic>/test.png

    COMMENT "Copying test texture..."
)
