# BtEmbedded

A static library for embedded devices to operate on Bluetooth devices (classic, not LE).

## Features

### Supported target platforms

BtEmbedded can be built on the following platforms:

- IN PROGRESS: Nintendo Wii
- PLANNED: Nintendo Wii's Starlet processor (cIOS)
- PLANNED: Linux

## Architecture and design goals

BtEmbedded tries to be portable and as fast and small as possible. The
structure of the code is as follows:

```
bt-embedded/
    backends/   # Platform backends
        wii.c   # Platform backend for the Nintendo Wii PPC processor
    backend.h   # Interface for platform backends
    
    drivers/    # Drivers for the Bluetooth controller
        wii.c   # Driver for the Wii's Bt controller
    drivers.h   # Interface for the Bluetooth drivers

    # BtEmbedded source code, common for all targets:
    hci.c
    hci.h
    ...
```

BtEmbedded is typically built statically, which means that the platform backend
and the Bluetooth driver are selected at build time.

The next two sections explain what code is expected for platform backends and
device drivers; the following sections describe other design decisions.

### Platform backends

A platform backend's main function is that of providing a function which can be
used to send data to the Bluetooth HCI controller, and delivering back HCI
events to BtEmbedded. The API operates on raw data buffers: the platform
backend does not need to perform any parsing of the data.

### Bluetooth drivers

A Bluetooth driver must bring up the HCI controller to a working state. This
can be as simple as invoking the HCI Reset command, or can require uploading
firmware or perform some vendor-specific commands. Drivers operate on the HCI
controller using the very same API that BtEmbedded provides to external
clients. However, they have also access to BtEmbedded's internal data
structures, so they alter some private parameters.

The reason why the Bluetooth driver is not integrated as part of the platform
backend is that while on most embedded platform there's a one to one relation
between platform and HCI controller, there can be platforms which support
different types of controllers; viceversa, it's also possible that the same
controller is found on different platforms (in the case of the Nintendo Wii, we
do have two different platforms, one for the PPC processor and the other one
for the ARM one, and they both work with the same bluetooth controller).

### Optimizing data transfers

BtEmbedded uses the BteBuffer structure (defined in
[buffer.h](bt-embedded/buffer.h)) for all data transfers between the HCI
controller and the client. This structure is reference counted linked list of
buffers: the reference count allows avoiding making copies of the data, because
the buffers will be freed only when the last reference is dropped.

The structure definition is public, therefore clients are free to allocate
buffer as they see fit (statically or dynamically), and its `free_func()`
method allows specifying a `free()`-like function that will be invoked at the
end of the buffer's lifetime.

### Optimizing for code size

The project is built with the GCC's `-ffunction-sections` option, which allows
the compiler to pick individual functions when linking the library into a
program; and BtEmbedded itself is written in such a way that the code for
parsing HCI events is passed to the core only in form a callback, without a
direct dependency. In practice, this means that if neither the client nor the
driver call the `bte_hci_read_local_name()` function, the code to parse the HCI
"Read Local Name" command result will not be included into the program's
binary.


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
