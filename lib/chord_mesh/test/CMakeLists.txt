
enable_testing()

include(GoogleTest)

# define unit tests

set(TEST_CASES
    cipher_tests.cpp
    handshake_tests.cpp
    insecure_stream_tests.cpp
    message_builder_tests.cpp
    message_parser_tests.cpp
    secure_stream_tests.cpp
    stream_acceptor_tests.cpp
    stream_connector_tests.cpp
    stream_io_tests.cpp
    )

# define test suite driver

add_executable(chord_mesh_testsuite
    ${TEST_CASES}
    base_mesh_fixture.cpp base_mesh_fixture.h
    test_mocks.h
    )
target_link_libraries(chord_mesh_testsuite PUBLIC
    chord::chord_mesh
    tempo::tempo_test
    gtest::gtest
    )
gtest_discover_tests(chord_mesh_testsuite)

# define test suite static library

add_library(ChordMeshTestSuite OBJECT
    ${TEST_CASES}
    base_mesh_fixture.cpp base_mesh_fixture.h
    test_mocks.h
    )
target_link_libraries(ChordMeshTestSuite PUBLIC
    chord::chord_mesh
    tempo::tempo_test
    gtest::gtest
    )
