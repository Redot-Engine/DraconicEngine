add_subdirectory(ThirdParty SYSTEM)

add_modules_library(Runtime SHARED)
# Runtime exposes the whole engine through the `draconic` module. Shell is linked
# PUBLIC (with the desktop backend, which supplies createShell()) so a consumer only
# links Runtime and does `import draconic;`.
target_link_libraries(Runtime PUBLIC Core Shell Shell_Desktop Scene Rendering)

# TODO: binding library