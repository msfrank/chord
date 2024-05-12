
# build ChordAgentRuntime static archive
add_library(ChordAgentRuntime OBJECT
    src/agent_service.cpp
    include/chord_agent/agent_service.h
    src/machine_logger.cpp
    include/chord_agent/machine_logger.h
    src/machine_process.cpp
    include/chord_agent/machine_process.h
    src/machine_supervisor.cpp
    include/chord_agent/machine_supervisor.h
    src/chord_agent.cpp
    include/chord_agent/chord_agent.h
    )

target_include_directories(ChordAgentRuntime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

if(${IS_DEBUG_BUILD})
    target_compile_definitions(ChordAgentRuntime PRIVATE
        "CHORD_LOCAL_MACHINE_EXECUTABLE=\"${CHORD_LOCAL_MACHINE_DIST_PATH}\"")
else()
    target_compile_definitions(ChordAgentRuntime PRIVATE
        "CHORD_LOCAL_MACHINE_EXECUTABLE=\"${CHORD_LOCAL_MACHINE_INSTALL_PATH}\"")
endif()

target_link_libraries(ChordAgentRuntime PUBLIC
    chord::chord_invoke
    chord::chord_protocol
    chord::chord_remoting
    lyric::lyric_packaging
    lyric::lyric_runtime
    tempo::tempo_command
    tempo::tempo_utils
    uv::uv
    )

# build chord-agent program
add_executable(chord-agent src/main.cpp)

# chord-agent depends on chord-local-machine
add_dependencies(chord-agent chord-local-machine)

target_include_directories(chord-agent PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

target_link_libraries(chord-agent PUBLIC ChordAgentRuntime)

# install targets
install(TARGETS chord-agent EXPORT chord-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

# add testing subdirectory
add_subdirectory(test)
