add_modules_library(Core PIC)
add_modules_library(Shell)
add_modules_library(Shell/Null)
add_modules_library(Shell/Desktop)
add_modules_library(Rendering PIC)
add_modules_library(Scene)

target_link_libraries(Core PUBLIC Definitions Math IO Memory Logging)
target_link_libraries(Shell PUBLIC Core)
# Shell backends: Null is headless, Desktop is SDL3-based. Neither is exported through
# the `draconic` module - each just supplies the createShell() factory (a compile-time
# bridge), and the SDL implementation is confined to the backend's own TUs. SDL3 stays
# PUBLIC so the desktop backend's own SDL-aware test still resolves SDL headers.
target_link_libraries(Shell_Null PUBLIC Shell)
target_link_libraries(Shell_Desktop PUBLIC Shell SDL3::SDL3-static)
target_link_libraries(Rendering PUBLIC RHI RenderGraph Mesh Material QuadRenderer Renderer)
target_link_libraries(Scene PUBLIC Renderable TransformComponent Camera)
