
# define unit tests

set(TEST_CASES
    manager_tests.cpp
)

# define test suite driver

add_executable(zuri_net_http_testsuite ${TEST_CASES})
add_dependencies(zuri_net_http_testsuite ZuriNetHttp)
target_include_directories(zuri_net_http_testsuite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}/../include
)
target_compile_definitions(zuri_net_http_testsuite PRIVATE
    "TESTER_CONFIG_PATH=\"${TESTER_CONFIG_PATH}\""
)
target_link_libraries(zuri_net_http_testsuite PUBLIC
    lyric::lyric_test
    tempo::tempo_test
    )
gtest_discover_tests(zuri_net_http_testsuite)

# define test suite static library

add_library(ZuriNetHttpTestSuite OBJECT ${TEST_CASES})
add_dependencies(ZuriNetHttpTestSuite ZuriNetHttp)
target_include_directories(ZuriNetHttpTestSuite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}/../include
)
target_compile_definitions(ZuriNetHttpTestSuite PRIVATE
    "TESTER_CONFIG_PATH=\"${TESTER_CONFIG_PATH}\""
)
target_link_libraries(ZuriNetHttpTestSuite PUBLIC
    lyric::lyric_test
    tempo::tempo_test
    )
