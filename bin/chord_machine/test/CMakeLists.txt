
enable_testing()

include(GoogleTest)

# define unit tests

set(TEST_CASES
    async_processor_tests.cpp
    async_queue_tests.cpp
    initialize_utils_tests.cpp
    local_machine_tests.cpp
)

set(TEST1_SPECIFIER "test1-${PROJECT_VERSION}@chord-machine-tests")
set(TEST1_ZPK "${CMAKE_CURRENT_BINARY_DIR}/data/chord-machine-tests_test1-${PROJECT_VERSION}.zpk")

add_subdirectory(data)

# define test suite driver

add_executable(chord_machine_testsuite ${TEST_CASES} test_mocks.h)
add_dependencies(chord_machine_testsuite chord-machine-test-data)
target_compile_definitions(chord_machine_testsuite PRIVATE
    "CHORD_MACHINE_EXECUTABLE=\"${CHORD_BUILD_CHORD_MACHINE_PATH}\""
    "TEST1_SPECIFIER=\"${TEST1_SPECIFIER}\""
    "TEST1_ZPK=\"${TEST1_ZPK}\""
    "ZURI_STD_PACKAGE_ZPK=\"${ZURI_INSTALL_PACKAGES_DIR}/${ZURI_STD_PACKAGE_ZPK}\""
)
target_link_libraries(chord_machine_testsuite PUBLIC
    ChordMachineRuntime
    lyric::lyric_bootstrap
    lyric::lyric_test
    tempo::tempo_test
    gtest::gtest
)
gtest_discover_tests(chord_machine_testsuite)

# define test suite static library

add_library(ChordMachineTestSuite OBJECT ${TEST_CASES} test_mocks.h)
add_dependencies(ChordMachineTestSuite chord-machine-test-data)
target_compile_definitions(ChordMachineTestSuite PRIVATE
    "CHORD_MACHINE_EXECUTABLE=\"${CHORD_BUILD_CHORD_MACHINE_PATH}\""
    "TEST1_SPECIFIER=\"${TEST1_SPECIFIER}\""
    "TEST1_ZPK=\"${TEST1_ZPK}\""
    "ZURI_STD_PACKAGE_ZPK=\"${ZURI_INSTALL_PACKAGES_DIR}/${ZURI_STD_PACKAGE_ZPK}\""
)
target_link_libraries(ChordMachineTestSuite PUBLIC
    ChordMachineRuntime
    lyric::lyric_bootstrap
    lyric::lyric_test
    tempo::tempo_test
    gtest::gtest
)
