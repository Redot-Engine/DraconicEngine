add_modules_library(Camera)
add_modules_library(TransformComponent)
add_modules_library(Renderable)

target_link_libraries(Camera PUBLIC Core Rendering bx)
target_link_libraries(TransformComponent PUBLIC Core)
target_link_libraries(Renderable PUBLIC TransformComponent Core Rendering)
