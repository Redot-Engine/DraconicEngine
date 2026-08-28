# DXC (DirectX Shader Compiler). The headers are used at compile time; the runtime
# library (dxcompiler.dll / libdxcompiler.so) is loaded on demand by draco.shaders
# via LoadLibrary/dlopen, so there is no import library to link.
#
# Draco::DXCHeaders carries the include dir AND the DRACO_DXC_PATH definition (the
# absolute path draco.shaders falls back to when no override is given), so a target
# that links it gets both.

add_library(DracoDXCHeaders INTERFACE)
add_library(Draco::DXCHeaders ALIAS DracoDXCHeaders)

target_include_directories(DracoDXCHeaders INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(WIN32)
    set(_dxc_runtime "${CMAKE_CURRENT_SOURCE_DIR}/lib/win-x64/dxcompiler.dll")
else()
    set(_dxc_runtime "${CMAKE_CURRENT_SOURCE_DIR}/lib/linux-x86_64/libdxcompiler.so")
endif()

target_compile_definitions(DracoDXCHeaders INTERFACE DRACO_DXC_PATH="${_dxc_runtime}")
