find_package(PkgConfig)

if(CMAKE_SYSTEM_NAME MATCHES "Starlet")
    set(PLATFORM_SOURCES
    )
elseif(CMAKE_SYSTEM_NAME MATCHES "NintendoWii")
    set(PLATFORM_SOURCES
        backends/wii.c
    )
    set(DRIVERS_SOURCES
        drivers/wii.c
    )
else()
    message(FATAL_ERROR "No available platform backend")
endif()

add_library(bt-embedded STATIC
    ${DRIVERS_SOURCES}
    ${PLATFORM_SOURCES}
    bte.c
    client.c
    hci.c
    hci_dev.c
)

target_include_directories(bt-embedded
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(bt-embedded PRIVATE
    -DBUILDING_BT_EMBEDDED
)

target_link_options(bt-embedded PRIVATE
    ${EXTRA_LDFLAGS}
)

target_link_libraries(bt-embedded PRIVATE
    ${EXTRA_LIBS}
    gcc
)
