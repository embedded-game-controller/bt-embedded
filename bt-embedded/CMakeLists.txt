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
    set(PC_EXTRA_LIBS "${PC_EXTRA_LIBS} -logc")
elseif(LibUSB_FOUND)
    list(APPEND EXTRA_LIBS PkgConfig::LibUSB)
    set(PLATFORM_SOURCES
        backends/libusb.c
    )
    set(DRIVERS_SOURCES
        drivers/generic.c
    )
    set(PC_EXTRA_REQUIRES "${PC_EXTRA_REQUIRES} libusb-1.0")
else()
    message(WARNING "No available platform backend")
endif()

if(OpenObex_FOUND)
    list(APPEND EXTRA_LIBS PkgConfig::OpenObex)
    list(APPEND EXTRA_SOURCES services/openobex.c)
    add_definitions(-DWITH_OPENOBEX)
endif()

if(WITH_DEBUG)
    list(APPEND EXTRA_DEFINITIONS -DWITH_DEBUG)
endif()

add_library(bt-embedded-objects OBJECT
    ${DRIVERS_SOURCES}
    ${PLATFORM_SOURCES}
    ${EXTRA_SOURCES}
    acl.c
    bte.c
    client.c
    hci.c
    hci_dev.c
    l2cap.c
    l2cap_echo.c
    l2cap_server.c
    services/obex.c
    services/sdp.c
)

target_include_directories(bt-embedded-objects
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(bt-embedded-objects PRIVATE
    -DBUILDING_BT_EMBEDDED
    ${EXTRA_DEFINITIONS}
)

target_link_options(bt-embedded-objects PRIVATE
    ${EXTRA_LDFLAGS}
)

target_link_libraries(bt-embedded-objects PRIVATE
    ${EXTRA_LIBS}
)

add_library(bt-embedded STATIC "")

target_link_libraries(bt-embedded PUBLIC
    bt-embedded-objects
)

set(PUBLIC_HEADERS
    bte.h
    buffer.h
    client.h
    hci.h
    hci_proto.h
    l2cap.h
    l2cap_server.h
    platform_defs.h
    types.h
)


# I didn't find a way to preserve the relative path, so installing these
# manually:
install(FILES
    services/hid.h
    services/hid_constants.h
    services/obex.h
    services/sdp.h
    services/sdp_constants.h
    DESTINATION "include/bt-embedded/services"
)

install(FILES
    backends/libusb.h
    backends/wii.h
    DESTINATION "include/bt-embedded/backends"
)

set_target_properties(bt-embedded PROPERTIES
    PUBLIC_HEADER "${PUBLIC_HEADERS}"
)

install(TARGETS bt-embedded
    LIBRARY
    ARCHIVE
    RUNTIME
    PUBLIC_HEADER DESTINATION "include/bt-embedded"
)

configure_file(../bt-embedded.pc.in bt-embedded.pc @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/bt-embedded.pc
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
