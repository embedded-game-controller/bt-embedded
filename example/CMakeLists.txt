add_executable(bte-example
    main.c
    terminal.c
)

target_include_directories(bte-example PRIVATE
    ..
)

target_link_libraries(bte-example PRIVATE
    bt-embedded
)
