add_modules_library(Core PIC)
add_modules_library(Shell)
add_modules_library(Rendering/Shaders)
add_modules_library(Rendering/RHI PIC)
add_modules_library(Rendering/RHI/Null)
add_modules_library(Rendering/RHI/Validation)
add_modules_library(Rendering/RHI/Vulkan)
if(WIN32)
    add_modules_library(Rendering/RHI/DX12)
endif()
add_modules_library(Rendering/Shaders/System)
add_modules_library(Rendering/Graphics)
add_modules_library(Rendering/RenderGraph)

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
# Rendering: RHI is the backend-agnostic interface module; Null is the headless
# no-op backend (for tests/tools). GPU backends (Vulkan, DX12) come later.
# Shaders: HLSL compilation via DXC. DXC's headers come from Draco::DXCHeaders
# (which also carries DRACO_DXC_PATH); the runtime library is dlopen'd, so only
# the platform's dl library is linked.
target_link_libraries(Rendering_Shaders PUBLIC Core Draco::DXCHeaders PRIVATE ${CMAKE_DL_LIBS})
# ShaderSystem: variant compile-on-demand + cache. Needs the RHI (to create GPU
# modules) on top of the RHI-free shaders module. Its test uses the Null backend.
target_link_libraries(Rendering_Shaders_System PUBLIC Rendering_Shaders Rendering_RHI Rendering_RHI_Null)
target_link_libraries(Rendering_RHI PUBLIC Core)
target_link_libraries(Rendering_RHI_Null PUBLIC Rendering_RHI)
# Validation wraps any backend; its test wraps the Null backend, so Null is
# PUBLIC here to give the test the rhi.null module interface.
target_link_libraries(Rendering_RHI_Validation PUBLIC Rendering_RHI Rendering_RHI_Null)
# Vulkan GPU backend. The Vulkan loader is PRIVATE: VkIncludes.h/vulkan.h live in
# the module's global fragment, so importers of rhi.vk don't need the headers, but
# the static lib's loader symbols still propagate to the final link.
target_link_libraries(Rendering_RHI_Vulkan PUBLIC Rendering_RHI PRIVATE Vulkan::Vulkan)
# DX12 GPU backend (Windows only). Links the D3D12/DXGI system libraries privately;
# IID_PPV_ARGS uses __uuidof, which Clang flags as a language extension.
if(WIN32)
    target_link_libraries(Rendering_RHI_DX12 PUBLIC Rendering_RHI PRIVATE d3d12 dxgi dxguid d3dcompiler)
    target_compile_options(Rendering_RHI_DX12 PRIVATE $<$<CXX_COMPILER_ID:Clang>:-Wno-language-extension-token>)
endif()
# Graphics render host: one target holding the graphics / graphics.null / graphics.gpu
# modules. The core `graphics` interface imports only base RHI + Shell; graphics.gpu
# pulls the backends. Shell is PUBLIC for the headless test. DX12 is added
# (with DRACO_HAS_DX12) only on Windows.
# RenderGraph: pass dependency resolution, transient aliasing, automatic barriers
# over the RHI. Backend-agnostic (base RHI only); Null is PUBLIC for its headless tests.
target_link_libraries(Rendering_RenderGraph PUBLIC Core Rendering_RHI Rendering_RHI_Null)
target_link_libraries(Rendering_Graphics PUBLIC
    Rendering_RHI Rendering_RHI_Null Rendering_RHI_Vulkan Rendering_RHI_Validation Shell)
if(WIN32)
    target_link_libraries(Rendering_Graphics PUBLIC Rendering_RHI_DX12)
    target_compile_definitions(Rendering_Graphics PUBLIC DRACO_HAS_DX12)
endif()
