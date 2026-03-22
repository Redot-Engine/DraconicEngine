include_guard(GLOBAL)

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED OUTPUT ERROR)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    if(IPO_SUPPORTED)
        message(STATUS "IPO / LTO enabled")
        add_link_options(-flto)
    else()
        message(STATUS "IPO / LTO not supported: <${ERROR}>")
    endif()
else()
    message(STATUS "IPO / LTO disabled")
    add_compile_definitions(DEBUG)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        # TODO: Make SIMD level configurable or detect at runtime
        add_compile_options(-mavx2 -mfma)
    endif()
endif()