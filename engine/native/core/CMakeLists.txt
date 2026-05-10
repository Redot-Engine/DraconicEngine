add_modules_library(definitions)
set_target_properties(definitions PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)
add_modules_library(math)
target_link_libraries(math PUBLIC definitions)