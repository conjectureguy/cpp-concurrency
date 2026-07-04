cmake_minimum_required(VERSION 3.20)

project(lockfree LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

FetchContent_Declare(
    benchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.9.1.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(
    googletest
    benchmark
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

# ----------------------------------------
# Benchmarks
# ----------------------------------------

add_executable(benchmark_spsc
    benchmarks/benchmark_spsc.cpp
)

target_link_libraries(benchmark_spsc
    PRIVATE
        lockfree
        benchmark::benchmark
)

# ----------------------------------------
# CTest
# ----------------------------------------

enable_testing()

include(GoogleTest)

gtest_discover_tests(test_spsc)