
# define unit tests

set(TEST_CASES
    helloworld_tests.cpp
)

# define test suite driver

add_executable(examples_demo_testsuite ${TEST_CASES})
add_dependencies(examples_demo_testsuite examples-demo)
target_include_directories(examples_demo_testsuite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}/../include
)
target_compile_definitions(examples_demo_testsuite PRIVATE
    "CHORD_DEMO_PACKAGE_PATH=\"${CHORD_DEMO_PACKAGE_PATH}\""
)
target_link_libraries(examples_demo_testsuite PUBLIC
    chord::chord_test
    lyric::lyric_test
    tempo::tempo_test
    )
gtest_discover_tests(examples_demo_testsuite)

# define test suite static library

add_library(ExamplesDemoTestSuite OBJECT ${TEST_CASES})
add_dependencies(ExamplesDemoTestSuite examples-demo)
target_include_directories(ExamplesDemoTestSuite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}/../include
)
target_compile_definitions(ExamplesDemoTestSuite PRIVATE
    "CHORD_DEMO_PACKAGE_PATH=\"${CHORD_DEMO_PACKAGE_PATH}\""
)
target_link_libraries(ExamplesDemoTestSuite PUBLIC
    chord::chord_test
    lyric::lyric_test
    tempo::tempo_test
    )
