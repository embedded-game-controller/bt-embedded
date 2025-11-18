cmake_minimum_required(VERSION 3.13)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(BTE_MAJOR 0)
set(BTE_MINOR 1)
set(BTE_PATCH 0)

project(
    bt-embedded
    VERSION ${BTE_MAJOR}.${BTE_MINOR}.${BTE_PATCH}
    LANGUAGES ASM C CXX
)

option(BUILD_EXAMPLE "Build example program" OFF)
option(BUILD_TESTS "Build and run tests" OFF)
option(ENABLE_COVERAGE "Enable code coverage rules" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

add_compile_options(
    ${ARCH}
    -O2
    -fomit-frame-pointer
    -ffunction-sections
    -Wall
    ${EXTRA_CFLAGS}
)

add_subdirectory(bt-embedded)

if(BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
