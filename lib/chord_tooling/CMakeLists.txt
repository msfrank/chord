
# do not run moc automatically
set(CMAKE_AUTOMOC OFF)

# build chord_tooling as a shared library
add_library(chord_tooling SHARED)
add_library(chord::chord_tooling ALIAS chord_tooling)

set(CHORD_TOOLING_INCLUDES
    include/chord_tooling/agent_store.h
    include/chord_tooling/chord_config.h
    include/chord_tooling/security_config.h
    include/chord_tooling/signer_store.h
    include/chord_tooling/tooling_conversions.h
    include/chord_tooling/tooling_result.h
    include/chord_tooling/zone.h
    include/chord_tooling/zone_config.h
    )
set_target_properties(chord_tooling PROPERTIES PUBLIC_HEADER "${CHORD_TOOLING_INCLUDES}")

target_sources(chord_tooling PRIVATE
    src/agent_store.cpp
    src/chord_config.cpp
    src/security_config.cpp
    src/signer_store.cpp
    src/tooling_conversions.cpp
    src/tooling_result.cpp
    src/zone.cpp
    src/zone_config.cpp
    )

# set the library version
set_target_properties(chord_tooling PROPERTIES VERSION "${FULL_VERSION}" SOVERSION "${MAJOR_VERSION}")

# set the RPATH if on OS X
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(chord_tooling PROPERTIES MACOSX_RPATH TRUE)
endif()

# set the public header include path differently on the target depending on the interface
target_include_directories(chord_tooling PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

set_target_properties(chord_tooling PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CHORD_BUILD_LIB_DIR}
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${LIB_RPATH}
    )

target_compile_definitions(chord_tooling PRIVATE
    "DISTRIBUTION_ROOT=\"${DISTRIBUTION_ROOT}\""
    "CONFIG_DIR_PREFIX=\"${CONFIG_DIR_PREFIX}\""
    "VENDOR_CONFIG_DIR_PREFIX=\"${VENDOR_CONFIG_DIR_PREFIX}\""
    )

target_link_libraries(chord_tooling
    PUBLIC
    chord::chord_common
    tempo::tempo_config
    tempo::tempo_utils
    zuri::zuri_tooling
    absl::flat_hash_map
    )

# install targets
install(TARGETS chord_tooling EXPORT chord-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/chord_tooling
    )

# add testing subdirectory
#add_subdirectory(test)
