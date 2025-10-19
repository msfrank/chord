
# run moc automatically when needed.
set(CMAKE_AUTOMOC OFF)

# build ChordRunRuntime static archive
add_library(ChordRunRuntime STATIC
    include/chord_run/create_session.h
    src/create_session.cpp
    include/chord_run/run_package_command.h
    src/run_package_command.cpp
    include/chord_run/run_result.h
    src/run_result.cpp
    include/chord_run/chord_run.h
    src/chord_run.cpp
    )

target_include_directories(ChordRunRuntime PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(DISTRIBUTION_ROOT ${CHORD_BUILD_OUTPUT_ROOT})
cmake_path(RELATIVE_PATH DISTRIBUTION_ROOT BASE_DIRECTORY ${CHORD_BUILD_BIN_DIR})

target_compile_definitions(ChordRunRuntime PRIVATE
    "PROJECT_VERSION=\"${PROJECT_VERSION}\""
    "DISTRIBUTION_ROOT=\"${DISTRIBUTION_ROOT}\""
    "CONFIG_DIR_PREFIX=\"${CONFIG_DIR_PREFIX}\""
    "VENDOR_CONFIG_DIR_PREFIX=\"${VENDOR_CONFIG_DIR_PREFIX}\""
    )

target_link_libraries(ChordRunRuntime
    chord::chord_sandbox
    tempo::tempo_command
    zuri::zuri_packager
    ${SANITIZER_LIBS}
    ${PROFILER_LIBS}
    )

# build chord program
add_executable(chord-run src/main.cpp)

set_target_properties(chord-run PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CHORD_BUILD_BIN_DIR}
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${BIN_RPATH}
    )

target_include_directories(chord-run PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(chord-run ChordRunRuntime)

install(TARGETS chord-run EXPORT chord-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )