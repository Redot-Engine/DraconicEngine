cmake_minimum_required(VERSION 4.0)

project(DraconicEngine LANGUAGES C CXX)

find_package(Git QUIET)
find_package(Threads REQUIRED)
find_package(Vulkan REQUIRED)

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

# Engine runtime (C++)
add_subdirectory(Engine/cpp)

# Samples (C++)
add_subdirectory(Samples/cpp)
