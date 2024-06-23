
# build ChordLocalMachineRuntime static library
add_library(ChordLocalMachineRuntime STATIC
    src/async_processor.cpp
    include/chord_local_machine/async_processor.h
    src/async_queue.cpp
    include/chord_local_machine/async_queue.h
    src/chord_local_machine.cpp
    include/chord_local_machine/chord_local_machine.h
    src/config_utils.cpp
    include/chord_local_machine/config_utils.h
    src/component_constructor.cpp
    include/chord_local_machine/component_constructor.h
    src/grpc_binder.cpp
    include/chord_local_machine/grpc_binder.h
    src/initialize_utils.cpp
    include/chord_local_machine/initialize_utils.h
    src/interpreter_runner.cpp
    include/chord_local_machine/interpreter_runner.h
    src/local_machine.cpp
    include/chord_local_machine/local_machine.h
    src/port_socket.cpp
    include/chord_local_machine/port_socket.h
    src/remoting_service.cpp
    include/chord_local_machine/remoting_service.h
    src/run_protocol_socket.cpp
    include/chord_local_machine/run_protocol_socket.h
    src/run_utils.cpp
    include/chord_local_machine/run_utils.h
    )

target_include_directories(ChordLocalMachineRuntime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

target_link_libraries(ChordLocalMachineRuntime PUBLIC
    chord::chord_invoke
    chord::chord_protocol
    chord::chord_remoting
    lyric::lyric_bootstrap
    lyric::lyric_packaging
    lyric::lyric_runtime
    tempo::tempo_command
    tempo::tempo_utils
    uv::uv
    )

# build chord-local-machine program
add_executable(chord-local-machine src/main.cpp)

target_include_directories(chord-local-machine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

set_target_properties(chord-local-machine PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${BIN_RPATH}
    )

target_link_libraries(chord-local-machine PUBLIC ChordLocalMachineRuntime)

# install targets
install(TARGETS chord-local-machine EXPORT chord-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

# add testing subdirectory
add_subdirectory(test)
