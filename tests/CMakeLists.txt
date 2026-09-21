set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp")
include(CTest)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

function(make_test TEST_NAME)
    target_link_libraries(${TEST_NAME}
        bt-embedded
        GTest::gtest_main
    )
    target_compile_definitions(${TEST_NAME} PRIVATE
        -DBUILDING_BT_EMBEDDED
    )
    gtest_discover_tests(${TEST_NAME})
endfunction()

#
# This tests most of the functionality of the library
#
add_executable(test_commands
    dummy_driver.c
    mock_backend.cpp
    test_buffer.cpp
    test_commands.cpp
    test_cpp_api.cpp
    test_data_matcher.cpp
    test_events.cpp
    test_getters.cpp
    test_hci_dev.cpp
    test_l2cap_config.cpp
    test_l2cap_connect.cpp
    test_l2cap_data.cpp
    test_l2cap_disconnect.cpp
    test_l2cap_echo.cpp
    test_l2cap_info.cpp
    test_l2cap_new_configured.cpp
    test_l2cap_server.cpp
    test_obex_discovery.cpp
    test_sdp.cpp
    test_sdp_de.cpp
)
make_test(test_commands)

#
# This is to test the case where some functionality of the library is not being
# reuqested; the goal is to verify that the resulting binary does not contain
# the symbols that are not used.
#
add_executable(test_minimal
    dummy_driver.c
    mock_backend.cpp
    test_minimal_l2cap.cpp
)
make_test(test_minimal)


set(UNIT_TESTS
    test_commands
    test_minimal
)

if(ENABLE_COVERAGE)
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

set(CHECKED_SYMBOLS
    acl_l2cap_handle_echo_req
)
add_test(NAME compare_sizes
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/check_symbol_sizes.sh
    test_minimal test_commands ${CHECKED_SYMBOLS}
)
