include(Compiler)
include(Modules)

add_modules_library(core SHARED)
target_link_libraries(core PUBLIC definitions math)
