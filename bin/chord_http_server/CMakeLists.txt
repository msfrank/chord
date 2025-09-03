
# build ChordHttpServerRuntime static archive
add_library(ChordHttpServerRuntime OBJECT
    src/http_acceptor.cpp
    include/chord_http_server/http_acceptor.h
    src/chord_http_server.cpp
    include/chord_http_server/chord_http_server.h
    src/http_service.cpp
    include/chord_http_server/http_service.h
    src/http_session.cpp
    include/chord_http_server/http_session.h
    )

target_include_directories(ChordHttpServerRuntime PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

target_link_libraries(ChordHttpServerRuntime PUBLIC
    chord::chord_protocol
    tempo::tempo_command
    tempo::tempo_utils
    zuri::zuri_packager
    Boost::headers
    uv::uv
    )

# build chord-http-server program
add_executable(chord-http-server src/main.cpp)

target_include_directories(chord-http-server PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

set_target_properties(chord-http-server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CHORD_BUILD_BIN_DIR}
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${BIN_RPATH}
    )

target_link_libraries(chord-http-server PUBLIC ChordHttpServerRuntime)

# install targets
install(TARGETS chord-http-server EXPORT chord-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

# add testing subdirectory
add_subdirectory(test)
