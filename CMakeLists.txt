cmake_minimum_required(VERSION 3.20)
project(Serde VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build as shared library instead of static" OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug or Release)" FORCE)
endif()

add_library(${PROJECT_NAME}
    include/Concepts.hpp
    include/Deserializer.hpp
    src/JSON.cpp
    include/JSON.hpp
    src/JsonParser.cpp
    include/JsonParser.hpp
    include/Load.hpp
    src/Log.cpp
    include/Log.hpp
    include/Reflectable.hpp
    include/ReflectionFactory.hpp
    include/Save.hpp
    include/Serializer.hpp
    include/Utils.hpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -O3 -DNDEBUG -finline-functions -fomit-frame-pointer
        -ffunction-sections -fdata-sections -flto -Wno-c99-designator
    )
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,-O2)
    set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>)
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        set(SANITIZER_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
        set(EXTRA_DEBUG_FLAGS "-g -O1")
        set(ASAN_OPTIONS_FLAGS "-fsanitize-recover=address -fsanitize=undefined -fsanitize-address-use-after-scope")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS} ${EXTRA_DEBUG_FLAGS} ${ASAN_OPTIONS_FLAGS}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS} ${EXTRA_DEBUG_FLAGS} ${ASAN_OPTIONS_FLAGS}")
        set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} ${SANITIZER_FLAGS} ${EXTRA_DEBUG_FLAGS} ${ASAN_OPTIONS_FLAGS}")
        add_compile_definitions(ASAN_OPTIONS="verbosity=1:halt_on_error=1:detect_leaks=1:symbolize=1")
    endif ()
endif ()

target_compile_definitions(${PROJECT_NAME} PUBLIC $<$<CONFIG:Debug>:DEBUG=1>)

add_subdirectory(HeaderForge)
include(${CMAKE_CURRENT_LIST_DIR}/SerdeHelpers.cmake)

# --------------------------
# Tests
# --------------------------

option(SERDE_BUILD_TESTS "Build JSON parser tests using JSONTestSuite" OFF)
option(SERDE_BUILD_BENCHMARK "Build JSON parser benchmark" OFF)

if(SERDE_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        JSONTestSuite
        GIT_REPOSITORY https://github.com/nst/JSONTestSuite.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JSONTestSuite)

    enable_testing()

    add_executable(json_test_suite tests/json_test_suite.cpp)
    target_link_libraries(json_test_suite PRIVATE ${PROJECT_NAME})

    add_test(
        NAME    JSONTestSuite
        COMMAND json_test_suite ${jsontestsuite_SOURCE_DIR}/test_parsing
    )
endif()

if(SERDE_BUILD_BENCHMARK)
    add_executable(benchmark tests/benchmark.cpp)
    target_link_libraries(benchmark PRIVATE ${PROJECT_NAME})
    target_compile_options(benchmark PRIVATE -O3 -DNDEBUG)

    set(BENCH_JSON     "${CMAKE_CURRENT_SOURCE_DIR}/tests/5MB.json"     CACHE PATH "Pretty JSON file for benchmark")
    set(BENCH_JSON_MIN "${CMAKE_CURRENT_SOURCE_DIR}/tests/5MB-min.json" CACHE PATH "Minified JSON file for benchmark")

    add_custom_target(run_benchmark
        COMMAND benchmark "${BENCH_JSON}" "${BENCH_JSON_MIN}"
        DEPENDS benchmark
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running JSON benchmark"
    )
endif()
