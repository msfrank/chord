
enable_testing()

include(GoogleTest)

# define unit tests

set(TEST_CASES
    async_processor_tests.cpp
    async_queue_tests.cpp
    initialize_utils_tests.cpp
    local_machine_tests.cpp
)

set(TEST1_SPECIFIER "test1-${PROJECT_VERSION}@chord-tests")
set(TEST1_ZPK "${CMAKE_CURRENT_BINARY_DIR}/data/chord-tests_test1-${PROJECT_VERSION}.zpk")

add_subdirectory(data)

# define test suite driver

add_executable(chord_local_machine_testsuite ${TEST_CASES} test_mocks.h)
add_dependencies(chord_local_machine_testsuite chord-local-machine-test-data)
target_compile_definitions(chord_local_machine_testsuite PRIVATE
    "CHORD_LOCAL_MACHINE_EXECUTABLE=\"${CHORD_LOCAL_MACHINE_DIST_PATH}\""
    "TEST1_SPECIFIER=\"${TEST1_SPECIFIER}\""
    "TEST1_ZPK=\"${TEST1_ZPK}\""
    "ZURI_STD_PACKAGE_ZPK=\"${ZURI_INSTALL_PACKAGES_DIR}/${ZURI_STD_PACKAGE_ZPK}\""
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
add_dependencies(chord_local_machine_testsuite chord-local-machine-test-data)
target_compile_definitions(ChordLocalMachineTestSuite PRIVATE
    "CHORD_LOCAL_MACHINE_EXECUTABLE=\"${CHORD_LOCAL_MACHINE_DIST_PATH}\""
    "TEST1_SPECIFIER=\"${TEST1_SPECIFIER}\""
    "TEST1_ZPK=\"${TEST1_ZPK}\""
    "ZURI_STD_PACKAGE_ZPK=\"${ZURI_INSTALL_PACKAGES_DIR}/${ZURI_STD_PACKAGE_ZPK}\""
)
target_link_libraries(ChordLocalMachineTestSuite PUBLIC
    ChordLocalMachineRuntime
    lyric::lyric_bootstrap
    lyric::lyric_test
    tempo::tempo_test
    gtest::gtest
)
