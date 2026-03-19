cmake_minimum_required(VERSION 3.14)
project(thread_pool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(thread_pool
    src/simple_thread_pool.cpp
)

target_include_directories(thread_pool
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(thread_pool PRIVATE -Wno-deprecated-declarations)
endif()

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(tests
    tests/test_simple_pool.cpp
    tests/test_work_stealing_deque.cpp
)

target_link_libraries(tests
    PRIVATE
        thread_pool
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(tests)