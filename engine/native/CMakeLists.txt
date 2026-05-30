add_subdirectory(thirdparty SYSTEM)

add_modules_library(platform)
add_modules_library(core SHARED)
add_modules_library(input)
add_modules_library(rendering)
add_modules_library(scene)

target_link_libraries(platform PUBLIC platform_impl)
target_link_libraries(core PUBLIC definitions math io memory)
target_link_libraries(input PRIVATE SDL3::SDL3-static platform core)
target_link_libraries(rendering PUBLIC rhi rendergraph mesh material quad_renderer renderer)
target_link_libraries(scene PUBLIC renderable transform_component camera)
