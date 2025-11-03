cmake_minimum_required(VERSION 3.13)

set(BTE_MAJOR 0)
set(BTE_MINOR 1)
set(BTE_PATCH 0)

project(
    bt-embedded
    VERSION ${BTE_MAJOR}.${BTE_MINOR}.${BTE_PATCH}
    LANGUAGES ASM C
)

option(BUILD_EXAMPLE "Build example program" OFF)

set(CMAKE_C_STANDARD 11)

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
