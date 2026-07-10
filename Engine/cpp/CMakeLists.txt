add_subdirectory(ThirdParty SYSTEM)

add_modules_library(Runtime SHARED)

target_link_libraries(Runtime PUBLIC Core Shell Shell_Desktop Scene Rendering)

# TODO: binding library