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
)

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
)
target_link_libraries(test_commands
    bt-embedded
    GTest::gtest_main
)
target_compile_definitions(test_commands PRIVATE
    -DBUILDING_BT_EMBEDDED
)

set(UNIT_TESTS
    test_commands
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
