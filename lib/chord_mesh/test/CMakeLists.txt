
enable_testing()

include(GoogleTest)

# define unit tests

set(TEST_CASES
    cipher_tests.cpp
    flood_mesh_tests.cpp
    handshake_tests.cpp
    insecure_stream_tests.cpp
    envelope_builder_tests.cpp
    envelope_parser_tests.cpp
    req_protocol_tests.cpp
    secure_stream_tests.cpp
    stream_acceptor_tests.cpp
    stream_connector_tests.cpp
    stream_io_tests.cpp
    )

# generate test messages
add_custom_command (
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/generated/test_messages.capnp.c++
      ${CMAKE_CURRENT_BINARY_DIR}/generated/test_messages.capnp.h
    COMMAND
      cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
    COMMAND
      ${CAPNP_EXECUTABLE} compile
      -I${CAPNP_INCLUDE_DIRECTORY}
      --src-prefix=${CMAKE_CURRENT_SOURCE_DIR}
      --output=${CAPNPC_CXX_EXECUTABLE}:${CMAKE_CURRENT_BINARY_DIR}/generated
      ${CMAKE_CURRENT_SOURCE_DIR}/test_messages.capnp
    DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/test_messages.capnp
)

# define test suite driver

add_executable(chord_mesh_testsuite
    ${TEST_CASES}
    base_mesh_fixture.cpp base_mesh_fixture.h
    test_mocks.h
    ${CMAKE_CURRENT_BINARY_DIR}/generated/test_messages.capnp.c++
    ${CMAKE_CURRENT_BINARY_DIR}/generated/test_messages.capnp.h
    )
target_include_directories(chord_mesh_testsuite PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
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
    ${CMAKE_CURRENT_BINARY_DIR}/generated/test_messages.capnp.c++
    ${CMAKE_CURRENT_BINARY_DIR}/generated/test_messages.capnp.h
    )
target_include_directories(chord_mesh_testsuite PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(ChordMeshTestSuite PUBLIC
    chord::chord_mesh
    tempo::tempo_test
    gtest::gtest
    )
