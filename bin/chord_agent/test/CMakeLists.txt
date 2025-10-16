
enable_testing()

include(GoogleTest)

# build mock-process program
add_executable(mock-process mock_process.cpp)

# define unit tests

set(TEST_CASES
    machine_process_tests.cpp
    machine_supervisor_tests.cpp
)

# define test suite driver

add_executable(chord_agent_testsuite ${TEST_CASES} test_mocks.h)
#add_dependencies(chord_agent_testsuite chord-local-machine examples-demo mock-process)
target_compile_definitions(chord_agent_testsuite PRIVATE
    "MOCK_PROCESS_EXECUTABLE=\"${CMAKE_CURRENT_BINARY_DIR}/mock-process\""
    "CHORD_LOCAL_MACHINE_EXECUTABLE=\"${CHORD_LOCAL_MACHINE_DIST_PATH}\""
    "EXAMPLES_DEMO_PACKAGE=\"${EXAMPLES_DEMO_PACKAGE}\""
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
#add_dependencies(chord_agent_testsuite chord-local-machine examples-demo mock-process)
target_compile_definitions(ChordAgentTestSuite PRIVATE
    "MOCK_PROCESS_EXECUTABLE=\"${CMAKE_CURRENT_BINARY_DIR}/mock-process\""
    "CHORD_LOCAL_MACHINE_EXECUTABLE=\"${CHORD_LOCAL_MACHINE_DIST_PATH}\""
    "EXAMPLES_DEMO_PACKAGE=\"${EXAMPLES_DEMO_PACKAGE}\""
)
target_link_libraries(ChordAgentTestSuite PUBLIC
    ChordAgentRuntime
    lyric::lyric_bootstrap
    lyric::lyric_test
    gtest::gtest
)
