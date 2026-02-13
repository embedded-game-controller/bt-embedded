if(OpenObex_FOUND)
    list(APPEND EXTRA_LIBS PkgConfig::OpenObex)
    add_definitions(-DWITH_OPENOBEX)
endif()

add_executable(bte-example
    main.c
    terminal.c
)

target_include_directories(bte-example PRIVATE
    ..
)

target_link_libraries(bte-example PRIVATE
    bt-embedded
    fat
)

add_executable(bte-obex
    obex.c
    terminal.c
)

target_include_directories(bte-obex PRIVATE
    ..
)

target_link_libraries(bte-obex PRIVATE
    ${EXTRA_LIBS}
    bt-embedded
    fat
)

add_executable(bte-wiimote
    wiimote.c
    terminal.c
)

target_include_directories(bte-wiimote PRIVATE
    ..
)

target_link_libraries(bte-wiimote PRIVATE
    ${EXTRA_LIBS}
    bt-embedded
    fat
)
