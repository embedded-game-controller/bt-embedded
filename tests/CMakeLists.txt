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

include(GoogleTest)
gtest_discover_tests(
    test_commands
)
