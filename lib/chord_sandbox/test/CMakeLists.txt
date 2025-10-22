
enable_testing()

include(GoogleTest)

# define unit tests

set(TEST_CASES
    chord_isolate_tests.cpp
    client_communication_stream_tests.cpp
    machine_utils_tests.cpp
    spawn_utils_tests.cpp
    )

set(TEST1_SPECIFIER "test1-${PROJECT_VERSION}@chord-sandbox-tests")
set(TEST1_ZPK "${CMAKE_CURRENT_BINARY_DIR}/data/chord-sandbox-tests_test1-${PROJECT_VERSION}.zpk")

add_subdirectory(data)

# define test suite driver

add_executable(chord_sandbox_testsuite ${TEST_CASES})
add_dependencies(chord_sandbox_testsuite chord-sandbox-test-data)
target_compile_definitions(chord_sandbox_testsuite PRIVATE
    "CHORD_AGENT_EXECUTABLE=\"${CHORD_BUILD_CHORD_AGENT_PATH}\""
    "TEST1_SPECIFIER=\"${TEST1_SPECIFIER}\""
    "TEST1_ZPK=\"${TEST1_ZPK}\""
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
add_dependencies(ChordSandboxTestSuite chord-sandbox-test-data)
target_compile_definitions(ChordSandboxTestSuite PRIVATE
    "CHORD_AGENT_EXECUTABLE=\"${CHORD_BUILD_CHORD_AGENT_PATH}\""
    "TEST1_SPECIFIER=\"${TEST1_SPECIFIER}\""
    "TEST1_ZPK=\"${TEST1_ZPK}\""
    "ZURI_INSTALL_CONFIG_DIR=\"${ZURI_INSTALL_CONFIG_DIR}\""
    "ZURI_INSTALL_VENDOR_CONFIG_DIR=\"${ZURI_INSTALL_VENDOR_CONFIG_DIR}\""
    )
target_link_libraries(ChordSandboxTestSuite PUBLIC
    chord::chord_sandbox
    chord::chord_test
    gtest::gtest
    )