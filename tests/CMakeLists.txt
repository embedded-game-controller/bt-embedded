include(CTest)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
FetchContent_MakeAvailable(googletest)

set(SRC ${CMAKE_SOURCE_DIR}/bt-embedded)
set(BTE_SOURCES
    ${SRC}/bte.c
    ${SRC}/client.c
    ${SRC}/hci.c
    ${SRC}/hci_dev.c
)

add_executable(test_commands
    ${BTE_SOURCES}
    dummy_driver.c
    mock_backend.cpp
    test_commands.cpp
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

    set(COVERAGE_LCOV_INCLUDES "${CMAKE_SOURCE_DIR}/bt-embedded/")
    setup_target_for_coverage_lcov(
        NAME check-code-coverage
        OUTPUT "${PROJECT_NAME}-coverage"
        TITLE "${PROJECT_NAME} Code Coverage"
        LCOV_ARGS "--ignore-errors;mismatch"
        EXECUTABLE ctest
        DEPENDENCIES ${UNIT_TESTS})
endif()
