
enable_testing()

include(GoogleTest)

# define unit tests

set(TEST_CASES
    async_processor_tests.cpp
    async_queue_tests.cpp
    initialize_utils_tests.cpp
    local_machine_tests.cpp
)

# define test suite driver

add_executable(chord_local_machine_testsuite ${TEST_CASES} test_mocks.h)
add_dependencies(chord_local_machine_testsuite examples-demo)
target_compile_definitions(chord_local_machine_testsuite PRIVATE
    "CHORD_LOCAL_MACHINE_EXECUTABLE=\"${CHORD_LOCAL_MACHINE_DIST_PATH}\""
    "CHORD_DEMO_PACKAGE_PATH=\"${CHORD_DEMO_PACKAGE_PATH}\""
)
target_link_libraries(chord_local_machine_testsuite PUBLIC
    ChordLocalMachineRuntime
    lyric::lyric_bootstrap
    lyric::lyric_test
    tempo::tempo_test
    gtest::gtest
)
gtest_discover_tests(chord_local_machine_testsuite)

# define test suite static library

add_library(ChordLocalMachineTestSuite OBJECT ${TEST_CASES} test_mocks.h)
add_dependencies(chord_local_machine_testsuite examples-demo)
target_compile_definitions(ChordLocalMachineTestSuite PRIVATE
    "CHORD_LOCAL_MACHINE_EXECUTABLE=\"${CHORD_LOCAL_MACHINE_DIST_PATH}\""
    "CHORD_DEMO_PACKAGE_PATH=\"${CHORD_DEMO_PACKAGE_PATH}\""
)
target_link_libraries(ChordLocalMachineTestSuite PUBLIC
    ChordLocalMachineRuntime
    lyric::lyric_bootstrap
    lyric::lyric_test
    tempo::tempo_test
    gtest::gtest
)
