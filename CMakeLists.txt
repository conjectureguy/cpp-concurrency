cmake_minimum_required(VERSION 3.20)

project(lockfree LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOCKFREE_ENABLE_TSAN "Build tests with ThreadSanitizer enabled" OFF)

# ----------------------------------------
# Header-only library
# ----------------------------------------

add_library(lockfree INTERFACE)

target_include_directories(lockfree
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ----------------------------------------
# Dependencies
# ----------------------------------------

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(
    googletest
)

# ----------------------------------------
# Tests
# ----------------------------------------

add_executable(test_spsc
    tests/test_spsc.cpp
)

target_link_libraries(test_spsc
    PRIVATE
        lockfree
        GTest::gtest_main
)

if(LOCKFREE_ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(test_spsc
            PRIVATE
                -fsanitize=thread
                -fno-omit-frame-pointer
                -g
        )
        target_link_options(test_spsc
            PRIVATE
                -fsanitize=thread
        )
    else()
        message(WARNING "LOCKFREE_ENABLE_TSAN is only configured for Clang and GCC")
    endif()
endif()

# ----------------------------------------
# Benchmarks
# ----------------------------------------

add_executable(benchmark_spsc
    benchmarks/benchmark_spsc.cpp
)

target_link_libraries(benchmark_spsc
    PRIVATE
        lockfree
)

# ----------------------------------------
# CTest
# ----------------------------------------

enable_testing()

include(GoogleTest)

gtest_discover_tests(test_spsc)
