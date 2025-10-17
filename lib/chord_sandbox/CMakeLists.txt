
# do not run moc automatically
set(CMAKE_AUTOMOC OFF)

# build chord_sandbox as a shared library
add_library(chord_sandbox SHARED)
add_library(chord::chord_sandbox ALIAS chord_sandbox)

set(CHORD_SANDBOX_INCLUDES
    include/chord_sandbox/chord_isolate.h
    include/chord_sandbox/grpc_connector.h
    include/chord_sandbox/remote_machine.h
    include/chord_sandbox/remoting_client.h
    include/chord_sandbox/run_protocol_plug.h
    include/chord_sandbox/sandbox_result.h
    include/chord_sandbox/sandbox_types.h
    )
set_target_properties(chord_sandbox PROPERTIES PUBLIC_HEADER "${CHORD_SANDBOX_INCLUDES}")

target_sources(chord_sandbox PRIVATE
    src/grpc_connector.cpp
    src/chord_isolate.cpp
    src/remote_machine.cpp
    src/remoting_client.cpp
    src/run_protocol_plug.cpp
    src/sandbox_result.cpp
    src/sandbox_types.cpp

    include/chord_sandbox/internal/machine_utils.h
    src/internal/machine_utils.cpp
    include/chord_sandbox/internal/spawn_utils.h
    src/internal/spawn_utils.cpp
    )

# set the library version
set_target_properties(chord_sandbox PROPERTIES VERSION "${FULL_VERSION}" SOVERSION "${MAJOR_VERSION}")

# set the RPATH if on OS X
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(chord_sandbox PROPERTIES MACOSX_RPATH TRUE)
endif()

# set the public header include path differently on the target depending on the interface
target_include_directories(chord_sandbox PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

if(${IS_DEBUG_BUILD})
    target_compile_definitions(chord_sandbox PRIVATE
        "CHORD_AGENT_EXECUTABLE=\"${CHORD_BUILD_CHORD_AGENT_PATH}\"")
else()
    target_compile_definitions(chord_sandbox PRIVATE
        "CHORD_AGENT_EXECUTABLE=\"${CHORD_RUNTIME_CHORD_AGENT_PATH}\"")
endif()

set_target_properties(chord_sandbox PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CHORD_BUILD_LIB_DIR}
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${LIB_RPATH}
    )

target_link_libraries(chord_sandbox
    PUBLIC
    chord::chord_invoke
    chord::chord_common
    chord::chord_remoting
    chord::chord_tooling
    lyric::lyric_common
    tempo::tempo_security
    tempo::tempo_utils
    absl::flat_hash_map
    gRPC::grpc++
    uv::uv
    )

# chord_sandbox depends on chord-agent
add_dependencies(chord_sandbox chord-agent)

# install targets
install(TARGETS chord_sandbox EXPORT chord-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/chord_sandbox
    )

# add testing subdirectory
add_subdirectory(test)
