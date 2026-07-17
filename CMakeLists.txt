cmake_minimum_required(VERSION 3.20)

project(cpp_concurrency LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CPP_CONCURRENCY_ENABLE_TSAN "Build tests with ThreadSanitizer enabled" OFF)

# ----------------------------------------
# Header-only library
# ----------------------------------------

add_library(cpp_concurrency INTERFACE)

target_include_directories(cpp_concurrency
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
        cpp_concurrency
        GTest::gtest_main
)

add_executable(test_lock_based_hashmap
    tests/test_lock_based_hashmap.cpp
    include/cpp_concurrency/hashmap_variants.h
    include/cpp_concurrency/hashmaps/lock_based_hashmap.h
    include/cpp_concurrency/hashmaps/lock_based_hashmap_vector.h
)

target_link_libraries(test_lock_based_hashmap
    PRIVATE
        cpp_concurrency
        GTest::gtest_main
)

if(CPP_CONCURRENCY_ENABLE_TSAN)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        foreach(test_target IN ITEMS test_spsc test_lock_based_hashmap)
            target_compile_options(${test_target}
                PRIVATE
                    -fsanitize=thread
                    -fno-omit-frame-pointer
                    -g
            )
            target_link_options(${test_target}
                PRIVATE
                    -fsanitize=thread
            )
        endforeach()
    else()
        message(WARNING "CPP_CONCURRENCY_ENABLE_TSAN is only configured for Clang and GCC")
    endif()
endif()

# ----------------------------------------
# Benchmarks
# ----------------------------------------

add_executable(benchmark_spsc
    benchmarks/benchmark_spsc.cpp
    include/cpp_concurrency/queues/spsc/lock_based_spsc_queue.h
    include/cpp_concurrency/queues/spsc/recursive_mutex_spsc_queue.h
    include/cpp_concurrency/queues/spsc/spsc_queue.h
    include/cpp_concurrency/spsc_variants.h
    include/cpp_concurrency/queues/spsc/optimize_memory_ordering_spsc_queue.h
    include/cpp_concurrency/queues/spsc/remove_false_sharing.h
)

target_link_libraries(benchmark_spsc
    PRIVATE
        cpp_concurrency
)

add_executable(benchmark_hashmap
    benchmarks/benchmark_hashmap.cpp
    include/cpp_concurrency/hashmap_variants.h
    include/cpp_concurrency/hashmaps/lock_based_hashmap.h
    include/cpp_concurrency/hashmaps/lock_based_hashmap_vector.h
)

target_link_libraries(benchmark_hashmap
    PRIVATE
        cpp_concurrency
)

# ----------------------------------------
# CTest
# ----------------------------------------

enable_testing()

include(GoogleTest)

gtest_discover_tests(test_spsc)
gtest_discover_tests(test_lock_based_hashmap)
