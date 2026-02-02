
project(chord_mesh)

# do not run moc automatically
set(CMAKE_AUTOMOC OFF)

# build chord_mesh as a shared library
add_library(chord_mesh SHARED)
add_library(chord::chord_mesh ALIAS chord_mesh)

# generate stream messages
add_custom_command (
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated/stream_messages.capnp.c++
      ${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated/stream_messages.capnp.h
    COMMAND
      cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated
    COMMAND
      ${CAPNP_EXECUTABLE} compile
      -I${CAPNP_INCLUDE_DIRECTORY}
      --src-prefix=${CMAKE_CURRENT_SOURCE_DIR}/share
      --output=${CAPNPC_CXX_EXECUTABLE}:${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated
      ${CMAKE_CURRENT_SOURCE_DIR}/share/stream_messages.capnp
    DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/share/stream_messages.capnp
)

# generate ensemble messages
add_custom_command (
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated/ensemble_messages.capnp.c++
      ${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated/ensemble_messages.capnp.h
    COMMAND
      cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated
    COMMAND
      ${CAPNP_EXECUTABLE} compile
      -I${CAPNP_INCLUDE_DIRECTORY}
      --src-prefix=${CMAKE_CURRENT_SOURCE_DIR}/share
      --output=${CAPNPC_CXX_EXECUTABLE}:${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/share/ensemble_messages.capnp
    DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/share/ensemble_messages.capnp
)

set(CHORD_MESH_INCLUDES
    include/chord_mesh/connect.h
    include/chord_mesh/flood.h
    include/chord_mesh/noise.h
    include/chord_mesh/mesh_result.h
    include/chord_mesh/message.h
    include/chord_mesh/envelope.h
    include/chord_mesh/req_protocol.h
    include/chord_mesh/stream.h
    include/chord_mesh/stream_buf.h
    include/chord_mesh/stream_acceptor.h
    include/chord_mesh/stream_connector.h
    include/chord_mesh/stream_io.h
    include/chord_mesh/stream_manager.h
    include/chord_mesh/stream_session.h
    )
set_target_properties(chord_mesh PROPERTIES PUBLIC_HEADER "${CHORD_MESH_INCLUDES}")

target_sources(chord_mesh PRIVATE
    src/connect.cpp
    src/flood.cpp
    src/mesh_result.cpp
    src/message.cpp
    src/noise.cpp
    src/envelope.cpp
    src/req_protocol.cpp
    src/stream.cpp
    src/stream_buf.cpp
    src/stream_acceptor.cpp
    src/stream_connector.cpp
    src/stream_io.cpp
    src/stream_manager.cpp
    src/stream_session.cpp

    # generated sources
    ${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated/ensemble_messages.capnp.c++
    ${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated/ensemble_messages.capnp.h
    ${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated/stream_messages.capnp.c++
    ${CMAKE_CURRENT_BINARY_DIR}/include/chord_mesh/generated/stream_messages.capnp.h
    )

# set the library version
set_target_properties(chord_mesh PROPERTIES VERSION "${FULL_VERSION}" SOVERSION "${MAJOR_VERSION}")

# set the RPATH if on OS X
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(chord_mesh PROPERTIES MACOSX_RPATH TRUE)
endif()

# set the public header include path differently on the target depending on the interface
target_include_directories(chord_mesh PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

target_compile_definitions(chord_mesh PRIVATE
    )

set_target_properties(chord_mesh PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CHORD_BUILD_LIB_DIR}
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${LIB_RPATH}
    )

target_link_libraries(chord_mesh
    PUBLIC
    chord::chord_common
    chord::chord_tooling
    tempo::tempo_security
    tempo::tempo_utils
    absl::flat_hash_map
    absl::random_random
    CapnProto::capnp
    noise-c::noise-c
    uv::uv
    )

# install targets
install(TARGETS chord_mesh EXPORT chord-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/chord_mesh
    )

# add testing subdirectory
add_subdirectory(test)
