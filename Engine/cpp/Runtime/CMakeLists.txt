add_modules_library(Core PIC)
add_modules_library(Shell)

target_link_libraries(Core PUBLIC Definitions Math IO Memory Logging)

# Shell is a single library: the interface plus both backends, separated only by folder
# (Desktop/ = SDL3, Null/ = headless). SDL3 is a PRIVATE implementation detail; nothing
# outside Shell sees it. createShell() dispatches to the backend chosen at compile time
# via DRACO_SHELL_SDL3 (default: the SDL3 desktop backend).
target_sources(Shell PUBLIC FILE_SET CXX_MODULES FILES
    Shell/Desktop/SDL3Shell.cppm Shell/Desktop/SDL3Window.cppm Shell/Desktop/SDL3Input.cppm
    Shell/Null/NullShell.cppm    Shell/Null/NullWindow.cppm    Shell/Null/NullInput.cppm)
target_sources(Shell PRIVATE
    Shell/Desktop/SDL3Shell.cpp Shell/Desktop/SDL3Window.cpp Shell/Desktop/SDL3Input.cpp
    Shell/Null/NullShell.cpp    Shell/Null/NullWindow.cpp    Shell/Null/NullInput.cpp)
target_link_libraries(Shell PUBLIC Core PRIVATE SDL3::SDL3-static)
target_compile_definitions(Shell PRIVATE DRACO_SHELL_SDL3=1)

# The backends live in subfolders, so their unit tests are registered explicitly (the
# module-library glob only sees the top level). Both link the one Shell library; the
# SDL3 test additionally uses SDL directly, so it links SDL3.
if(CMAKE_TESTING_ENABLED)
    add_executable(Shell_NullShell_test Shell/Null/NullShell.test.cpp)
    add_executable(Shell_SDL3Shell_test Shell/Desktop/SDL3Shell.test.cpp)
    target_link_libraries(Shell_SDL3Shell_test PRIVATE SDL3::SDL3-static)
    foreach(SHELL_TEST_TARGET Shell_NullShell_test Shell_SDL3Shell_test)
        set_target_properties(${SHELL_TEST_TARGET} PROPERTIES CXX_SCAN_FOR_MODULES ON)
        target_compile_features(${SHELL_TEST_TARGET} PUBLIC cxx_std_23)
        target_include_directories(${SHELL_TEST_TARGET} PUBLIC ${DOCTEST_INCLUDE_DIR})
        target_link_libraries(${SHELL_TEST_TARGET} PRIVATE Shell)
        add_test(NAME ${SHELL_TEST_TARGET} COMMAND ${SHELL_TEST_TARGET} --reporter junit --out "Testing/${SHELL_TEST_TARGET}.xml")
    endforeach()
endif()
