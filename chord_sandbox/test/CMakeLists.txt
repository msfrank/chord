
enable_testing()

include(GoogleTest)

# define unit tests

set(TEST_CASES
    chord_isolate_tests.cpp
    client_communication_stream_tests.cpp
    machine_utils_tests.cpp
    spawn_utils_tests.cpp
    )

# define test suite driver

add_executable(chord_sandbox_testsuite ${TEST_CASES})
target_compile_definitions(chord_sandbox_testsuite PRIVATE
    "CHORD_AGENT_EXECUTABLE=\"${CHORD_AGENT_DIST_PATH}\""
    "ZURI_INSTALL_CONFIG_DIR=\"${ZURI_INSTALL_CONFIG_DIR}\""
    "ZURI_INSTALL_VENDOR_CONFIG_DIR=\"${ZURI_INSTALL_VENDOR_CONFIG_DIR}\""
    )
target_link_libraries(chord_sandbox_testsuite PUBLIC
    chord::chord_sandbox
    chord::chord_test
    gtest::gtest
    )
gtest_discover_tests(chord_sandbox_testsuite)

# define test suite static library

add_library(ChordSandboxTestSuite OBJECT ${TEST_CASES})
target_compile_definitions(ChordSandboxTestSuite PRIVATE
    "CHORD_AGENT_EXECUTABLE=\"${CHORD_AGENT_DIST_PATH}\""
    "ZURI_INSTALL_CONFIG_DIR=\"${ZURI_INSTALL_CONFIG_DIR}\""
    "ZURI_INSTALL_VENDOR_CONFIG_DIR=\"${ZURI_INSTALL_VENDOR_CONFIG_DIR}\""
    )
target_link_libraries(ChordSandboxTestSuite PUBLIC
    chord::chord_sandbox
    chord::chord_test
    gtest::gtest
    )