set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp")
include(CTest)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
FetchContent_MakeAvailable(googletest)

set(SRC ${CMAKE_SOURCE_DIR}/bt-embedded)
set(BTE_SOURCES
    ${SRC}/acl.c
    ${SRC}/bte.c
    ${SRC}/client.c
    ${SRC}/hci.c
    ${SRC}/hci_dev.c
    ${SRC}/l2cap.c
    ${SRC}/l2cap_server.c
)

#
# This tests most of the functionality of the library
#
add_executable(test_commands
    ${BTE_SOURCES}
    dummy_driver.c
    mock_backend.cpp
    test_buffer.cpp
    test_commands.cpp
    test_cpp_api.cpp
    test_data_matcher.cpp
    test_events.cpp
    test_hci_dev.cpp
    test_l2cap_config.cpp
    test_l2cap_connect.cpp
    test_l2cap_data.cpp
    test_l2cap_disconnect.cpp
    test_l2cap_echo.cpp
    test_l2cap_server.cpp
)
target_link_libraries(test_commands
    bt-embedded
    GTest::gtest_main
)
target_compile_definitions(test_commands PRIVATE
    -DBUILDING_BT_EMBEDDED
)

#
# This is to test the case where some functionality of the library is not being
# reuqested; the goal is to verify that the resulting binary does not contain
# the symbols that are not used.
#
add_executable(test_minimal
    ${BTE_SOURCES}
    dummy_driver.c
    mock_backend.cpp
    test_minimal_l2cap.cpp
)
target_link_libraries(test_minimal
    bt-embedded
    GTest::gtest_main
)
target_compile_definitions(test_minimal PRIVATE
    -DBUILDING_BT_EMBEDDED
)


set(UNIT_TESTS
    test_commands
    test_minimal
)
include(GoogleTest)
gtest_discover_tests(${UNIT_TESTS})

if(ENABLE_COVERAGE)
    include(CodeCoverage)
    append_coverage_compiler_flags()

    set(COVERAGE_EXCLUDES
        "${CMAKE_SOURCE_DIR}/tests/"
        "${CMAKE_BINARY_DIR}"
    )
    setup_target_for_coverage_gcovr_html(
        NAME check-code-coverage
        OUTPUT "${PROJECT_NAME}-coverage"
        TITLE "${PROJECT_NAME} Code Coverage"
        EXECUTABLE ctest
        DEPENDENCIES ${UNIT_TESTS})
endif()
