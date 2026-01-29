
# build ChordMachineRuntime static library
add_library(ChordMachineRuntime STATIC
    src/async_processor.cpp
    include/chord_machine/async_processor.h
    src/async_queue.cpp
    include/chord_machine/async_queue.h
    src/chord_machine.cpp
    include/chord_machine/chord_machine.h
    src/config_utils.cpp
    include/chord_machine/config_utils.h
    src/component_constructor.cpp
    include/chord_machine/component_constructor.h
    src/grpc_binder.cpp
    include/chord_machine/grpc_binder.h
    src/initialize_utils.cpp
    include/chord_machine/initialize_utils.h
    src/interpreter_runner.cpp
    include/chord_machine/interpreter_runner.h
    src/local_machine.cpp
    include/chord_machine/local_machine.h
    src/machine_result.cpp
    include/chord_machine/machine_result.h
    src/port_socket.cpp
    include/chord_machine/port_socket.h
    src/remoting_service.cpp
    include/chord_machine/remoting_service.h
    src/run_protocol_socket.cpp
    include/chord_machine/run_protocol_socket.h
    src/run_utils.cpp
    include/chord_machine/run_utils.h
    )

target_include_directories(ChordMachineRuntime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

target_link_libraries(ChordMachineRuntime PUBLIC
    chord::chord_common
    chord::chord_mesh
    lyric::lyric_bootstrap
    lyric::lyric_runtime
    tempo::tempo_command
    tempo::tempo_utils
    zuri::zuri_distributor
    zuri::zuri_packager
    uv::uv
    )

# build chord-machine program
add_executable(chord-machine src/main.cpp)

target_include_directories(chord-machine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

set_target_properties(chord-machine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CHORD_BUILD_BIN_DIR}
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${BIN_RPATH}
    )

target_link_libraries(chord-machine PUBLIC ChordMachineRuntime)

# install targets
install(TARGETS chord-machine EXPORT chord-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

# add testing subdirectory
add_subdirectory(test)
