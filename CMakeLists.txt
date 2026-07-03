cmake_minimum_required(VERSION 3.20)

project(lockfree LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(test_spsc
    tests/test_spsc.cpp
)

target_include_directories(test_spsc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()

add_test(
    NAME SPSCQueueTests
    COMMAND test_spsc
)