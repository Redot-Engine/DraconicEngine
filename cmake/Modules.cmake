include_guard(GLOBAL)

set(NATIVE_SOURCE_DIR "${PROJECT_SOURCE_DIR}/engine/native")

set(NATIVE_THIRD_PARTY_DIR "${NATIVE_SOURCE_DIR}/thirdparty")

if (BUILD_TESTING)
    message(STATUS "Bootstrapping unit tests module boost.ut")
    add_library(boost_ut_main "${NATIVE_THIRD_PARTY_DIR}/boost/ut_main.cpp")
    target_sources(boost_ut_main
        PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS "${NATIVE_THIRD_PARTY_DIR}/boost"
        FILES "${NATIVE_THIRD_PARTY_DIR}/boost/ut.cppm"
    )
    target_compile_features(boost_ut_main PUBLIC cxx_std_23)
endif()

function(add_modules_library)
    cmake_parse_arguments(
            MOD_LIB # prefix for all variables
            "STATIC;SHARED" # tags for flags (only defined ones will be true)
            "" # tags for single values
            "" # tags for lists
            "${ARGN}"
    )

    set(LIB_PATH ${ARGV0})

    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${LIB_PATH}")
        message(FATAL_ERROR "Library directory ${LIB_PATH} not found")
    endif()

    set(LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${LIB_PATH}")

    if (EXISTS "${LIB_DIR}/CMakeLists.txt") # allow recursion
        add_subdirectory(${LIB_DIR})
    endif()

    string(MAKE_C_IDENTIFIER ${LIB_PATH} LIB_TARGET)
    file(GLOB CPP_MODULE_FILES CONFIGURE_DEPENDS "${LIB_PATH}/*.cppm")
    file(GLOB CPP_UNIT_TESTS CONFIGURE_DEPENDS "${LIB_PATH}/*.test.cpp")
    file(GLOB CPP_SRC_FILES CONFIGURE_DEPENDS "${LIB_PATH}/*.cpp")
    if (CPP_UNIT_TESTS)
        list(REMOVE_ITEM CPP_SRC_FILES ${CPP_UNIT_TESTS})
    endif()

    if (MOD_LIB_SHARED)
        message(STATUS "Adding shared modules library ${LIB_TARGET}")
        add_library(${LIB_TARGET} SHARED)
    else()
        message(STATUS "Adding static modules library ${LIB_TARGET}")
        add_library(${LIB_TARGET} STATIC)
    endif()
    target_compile_features(${LIB_TARGET} PUBLIC cxx_std_23)
    target_include_directories(${LIB_TARGET} PUBLIC ${NATIVE_SOURCE_DIR})

    target_sources(${LIB_TARGET}
        PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${LIB_DIR}
        FILES ${CPP_MODULE_FILES}
    )

    target_sources(${LIB_TARGET} PRIVATE ${CPP_SRC_FILES})

    if(CMAKE_TESTING_ENABLED)
        foreach(UNIT_TEST_FILE ${CPP_UNIT_TESTS})
            string(REPLACE "${LIB_DIR}/" "" UNIT_TEST_TARGET "${UNIT_TEST_FILE}")
            string(REPLACE ".test.cpp" "_test" UNIT_TEST_TARGET ${UNIT_TEST_TARGET})
            string(MAKE_C_IDENTIFIER ${UNIT_TEST_TARGET} UNIT_TEST_TARGET)
            if (NOT UNIT_TEST_TARGET MATCHES ".*${LIB_TARGET}.*")
                string(PREPEND UNIT_TEST_TARGET "${LIB_TARGET}_")
            endif()
            add_executable(${UNIT_TEST_TARGET} ${UNIT_TEST_FILE})
            target_compile_features(${UNIT_TEST_TARGET} PUBLIC cxx_std_23)
            target_link_libraries(${UNIT_TEST_TARGET} PRIVATE boost_ut_main ${LIB_TARGET})
            message(STATUS "Unit test ${UNIT_TEST_TARGET}")
            add_test(NAME ${UNIT_TEST_TARGET} COMMAND ${UNIT_TEST_TARGET} --reporter junit --out "Testing/${UNIT_TEST_TARGET}.xml")
        endforeach()
    endif()

endfunction()

function(target_link_modules)
    cmake_parse_arguments(
        MOD_LINK # prefix for all variables
        "" # tags for flags (only defined ones will be true)
        "" # tags for single values
        "PRIVATE;PUBLIC" # tags for lists
        "${ARGN}"
    )

    if (MOD_LINK_PUBLIC)
        foreach(NAME ${MOD_LINK_PUBLIC})
            set(DIR "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}")
            string(MAKE_C_IDENTIFIER ${NAME} TARGET)
            target_link_libraries(${ARGV0} PUBLIC ${TARGET})
        endforeach()
    endif()

    if (MOD_LINK_PRIVATE)
        foreach(NAME ${MOD_LINK_PRIVATE})
            set(DIR "${CMAKE_CURRENT_SOURCE_DIR}/${NAME}")
            string(MAKE_C_IDENTIFIER ${NAME} TARGET)
            target_link_libraries(${ARGV0} PRIVATE ${TARGET})
        endforeach()
    endif()

endfunction()