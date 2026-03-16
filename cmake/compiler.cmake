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
endif()

# temporarily adding this in, we can fix this later.
add_compile_options(-mavx2 -mfma)
