# PIC: these are linked into the shared Runtime library (via Core), so their objects
# must be position-independent - a non-PIC static (e.g. Quaternion::identity) otherwise
# fails to relocate when making the shared object.
add_modules_library(Definitions PIC)
add_modules_library(Math PIC)
add_modules_library(IO PIC)
add_modules_library(Memory PIC)
add_modules_library(Logging PIC)

target_link_libraries(Math PUBLIC Definitions)
target_link_libraries(IO PUBLIC Definitions)
target_link_libraries(Memory PUBLIC Definitions Math)
target_link_libraries(Logging PUBLIC Definitions)
