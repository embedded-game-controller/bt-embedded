# BtEmbedded

A static library for embedded devices to operate on Bluetooth devices (classic, not LE).

## Features

### Supported target platforms

BtEmbedded can be built on the following platforms:

- IN PROGRESS: Nintendo Wii
- PLANNED: Nintendo Wii's Starlet processor (cIOS)
- PLANNED: Linux

## Compilation

### Nintendo Wii

##### 1) Install `devkitPPC`

- Download and install [devkitPPC](https://devkitpro.org/wiki/Getting_Started)
- Make sure to install the `devkitppc-cmake` package when using `pacman`

##### 3) Build `libbt-embedded.a`

1. `mkdir build && cd build`
2. Configure it with CMake:
  &ensp; `cmake -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Wii.cmake" -DBUILD_EXAMPLE=ON ..`
3. `make` (or `ninja` if configured with `-G Ninja`)
4. `libbt-embedded.a` will be generated

## Credits

- [libogc's lwBT](https://github.com/devkitPro/libogc/tree/master/lwbt), which
  has been used as a base for this work.
