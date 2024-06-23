
# do not run moc automatically
set(CMAKE_AUTOMOC OFF)

# build chord_protocol as a shared library
add_library(chord_protocol SHARED)
add_library(chord::chord_protocol ALIAS chord_protocol)

set(CHORD_PROTOCOL_INCLUDES
    include/chord_protocol/abstract_protocol_handler.h
    include/chord_protocol/abstract_protocol_writer.h
    include/chord_protocol/protocol_types.h
    )
set_target_properties(chord_protocol PROPERTIES PUBLIC_HEADER "${CHORD_PROTOCOL_INCLUDES}")

target_sources(chord_protocol PRIVATE
    src/protocol_types.cpp
    )

# set the library version
set_target_properties(chord_protocol PROPERTIES VERSION "${FULL_VERSION}" SOVERSION "${MAJOR_VERSION}")

# set the RPATH if on OS X
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(chord_protocol PROPERTIES MACOSX_RPATH TRUE)
endif()

# set the public header include path differently on the target depending on the interface
target_include_directories(chord_protocol PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

set_target_properties(chord_protocol PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${LIB_RPATH}
    )

target_link_libraries(chord_protocol
    PUBLIC
    tempo::tempo_security
    tempo::tempo_utils
    absl::flat_hash_map
    gRPC::grpc++
    uv::uv
    )

# install targets
install(TARGETS chord_protocol EXPORT chord-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/chord_protocol
    )

# add testing subdirectory
#add_subdirectory(test)
