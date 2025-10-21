
# do not run moc automatically
set(CMAKE_AUTOMOC OFF)

# build chord_common as a shared library
add_library(chord_common SHARED)
add_library(chord::chord_common ALIAS chord_common)

set(CHORD_COMMON_INCLUDES
    include/chord_common/abstract_certificate_signer.h
    include/chord_common/abstract_protocol_handler.h
    include/chord_common/abstract_protocol_writer.h
    include/chord_common/common_conversions.h
    include/chord_common/common_types.h
    include/chord_common/transport_location.h
    )
set_target_properties(chord_common PROPERTIES PUBLIC_HEADER "${CHORD_COMMON_INCLUDES}")

target_sources(chord_common PRIVATE
    src/common_conversions.cpp
    src/common_types.cpp
    src/transport_location.cpp
    )

# set the library version
set_target_properties(chord_common PROPERTIES VERSION "${FULL_VERSION}" SOVERSION "${MAJOR_VERSION}")

# set the RPATH if on OS X
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(chord_common PROPERTIES MACOSX_RPATH TRUE)
endif()

# set the public header include path differently on the target depending on the interface
target_include_directories(chord_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

set_target_properties(chord_common PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CHORD_BUILD_LIB_DIR}
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${LIB_RPATH}
    )

target_link_libraries(chord_common
    PUBLIC
    tempo::tempo_config
    tempo::tempo_security
    tempo::tempo_utils
    absl::flat_hash_map
    PRIVATE
    Boost::headers
    gRPC::grpc++
    uv::uv
    )

# install targets
install(TARGETS chord_common EXPORT chord-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/chord_common
    )

# add testing subdirectory
#add_subdirectory(test)
