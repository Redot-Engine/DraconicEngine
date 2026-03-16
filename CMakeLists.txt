cmake_minimum_required(VERSION 4.2)
#4.0: set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "a9e1cf81-9932-4810-974b-6eccaf14e457")
#4.2: set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")
project(DraconicEngine LANGUAGES C CXX)

enable_testing()

include(cmake/compiler.cmake)
include(cmake/modules.cmake)

add_modules_library(core SHARED)
target_link_libraries(core PUBLIC definitions math)
