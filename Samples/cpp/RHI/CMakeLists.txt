# Draconic RHI samples + their shared framework (GPU/display dependent - built,
# not run in CI).

# Shared sample framework: window bring-up (SDL3 shell), backend/device/swapchain,
# main loop, shader-compile helpers. Links every RHI backend + the shader compiler.
# DX12 is Windows-only (DRACO_HAS_DX12).
add_modules_library(Framework)
target_link_libraries(Framework PUBLIC
    Core
    Rendering_RHI
    Rendering_RHI_Vulkan
    Rendering_RHI_Validation
    Rendering_Shaders
    Shell)
if(WIN32)
    target_link_libraries(Framework PUBLIC Rendering_RHI_DX12)
    target_compile_definitions(Framework PUBLIC DRACO_HAS_DX12)
endif()

# Each sample is one Main.cpp linking the framework. SDL3 is static, so its
# constructor-registered video/input drivers must be force-loaded (whole archive)
# at the executable link or they get stripped.
function(draco_add_rhi_sample name)
    add_executable(${name} Main.cpp)
    target_link_libraries(${name} PRIVATE Framework)
    target_compile_options(${name} PRIVATE
        "$<$<CXX_COMPILER_ID:Clang,GNU>:-Wno-error=unused-parameter;-Wno-error=unused-variable;-Wno-error=missing-field-initializers;-Wno-error=misleading-indentation>")
    if(UNIX AND NOT APPLE)
        target_link_libraries(${name} PRIVATE Threads::Threads dl)
        target_link_options(${name} PRIVATE -Wl,--whole-archive $<TARGET_FILE:SDL3::SDL3-static> -Wl,--no-whole-archive)
    elseif(APPLE)
        target_link_options(${name} PRIVATE -Wl,-force_load,$<TARGET_FILE:SDL3::SDL3-static>)
    elseif(MSVC)
        target_link_options(${name} PRIVATE /WHOLEARCHIVE:$<TARGET_FILE:SDL3::SDL3-static>)
    endif()
endfunction()

add_subdirectory(Sample001_Triangle)
add_subdirectory(Sample002_Textures)
add_subdirectory(Sample003_UniformBuffers)
add_subdirectory(Sample004_Compute)
add_subdirectory(Sample005_BindGroups)
add_subdirectory(Sample006_Blending)
add_subdirectory(Sample007_Instancing)
add_subdirectory(Sample008_DepthBuffer)
add_subdirectory(Sample009_Mipmaps)
add_subdirectory(Sample010_MSAA)
add_subdirectory(Sample011_MRT)
add_subdirectory(Sample012_Wireframe)
add_subdirectory(Sample013_BorderSampler)
add_subdirectory(Sample014_Blit)
add_subdirectory(Sample015_Queries)
add_subdirectory(Sample016_Readback)
add_subdirectory(Sample017_MultiQueue)
add_subdirectory(Sample018_Bindless)
add_subdirectory(Sample019_BatchUpload)
add_subdirectory(Sample020_MeshShaders)
add_subdirectory(Sample021_RayTracing)
add_subdirectory(Sample022_StencilOutline)
add_subdirectory(Sample023_CubeMap)
add_subdirectory(Sample024_OcclusionQuery)
add_subdirectory(Sample025_MultiDrawIndirect)
add_subdirectory(Sample026_DynamicOffsets)
add_subdirectory(Sample027_3DTexture)
add_subdirectory(Sample028_ProceduralRT)
add_subdirectory(Sample029_ResolveTexture)
add_subdirectory(Sample030_RenderBundles)
add_subdirectory(Smoketest)
