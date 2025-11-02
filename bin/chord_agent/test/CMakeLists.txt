
enable_testing()

include(GoogleTest)

# build mock-process program
add_executable(mock-process mock_process.cpp)

# define unit tests

set(TEST_CASES
    machine_process_tests.cpp
    machine_supervisor_tests.cpp
)

set(TEST1_SPECIFIER "test1-${PROJECT_VERSION}@chord-machine-tests")
set(TEST1_ZPK "${CMAKE_CURRENT_BINARY_DIR}/data/chord-machine-tests_test1-${PROJECT_VERSION}.zpk")

add_subdirectory(data)

# define test suite driver

add_executable(chord_agent_testsuite ${TEST_CASES} test_mocks.h)
add_dependencies(chord_agent_testsuite chord-machine chord-agent-test-data mock-process)
target_compile_definitions(chord_agent_testsuite PRIVATE
    "CHORD_MACHINE_EXECUTABLE=\"${CHORD_BUILD_CHORD_MACHINE_PATH}\""
    "TEST1_SPECIFIER=\"${TEST1_SPECIFIER}\""
    "TEST1_ZPK=\"${TEST1_ZPK}\""
    "MOCK_PROCESS_EXECUTABLE=\"${CMAKE_CURRENT_BINARY_DIR}/mock-process\""
)
target_link_libraries(chord_agent_testsuite PUBLIC
    ChordAgentRuntime
    lyric::lyric_bootstrap
    lyric::lyric_test
    gtest::gtest
)
gtest_discover_tests(chord_agent_testsuite)

# define test suite static library

add_library(ChordAgentTestSuite OBJECT ${TEST_CASES} test_mocks.h)
add_dependencies(chord_agent_testsuite chord-machine chord-agent-test-data mock-process)
target_compile_definitions(ChordAgentTestSuite PRIVATE
    "CHORD_MACHINE_EXECUTABLE=\"${CHORD_BUILD_CHORD_MACHINE_PATH}\""
    "TEST1_SPECIFIER=\"${TEST1_SPECIFIER}\""
    "TEST1_ZPK=\"${TEST1_ZPK}\""
    "MOCK_PROCESS_EXECUTABLE=\"${CMAKE_CURRENT_BINARY_DIR}/mock-process\""
)
target_link_libraries(ChordAgentTestSuite PUBLIC
    ChordAgentRuntime
    lyric::lyric_bootstrap
    lyric::lyric_test
    gtest::gtest
)
