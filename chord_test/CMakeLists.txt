
# do not run moc automatically
set(CMAKE_AUTOMOC OFF)

# build chord_test as a shared library
add_library(chord_test SHARED)
add_library(chord::chord_test ALIAS chord_test)

set(CHORD_TEST_INCLUDES
    include/chord_test/chord_sandbox_tester.h
    include/chord_test/test_result.h
    )
set_target_properties(chord_test PROPERTIES PUBLIC_HEADER "${CHORD_TEST_INCLUDES}")

target_sources(chord_test PRIVATE
    src/chord_sandbox_tester.cpp
    src/test_result.cpp
    )

# set the library version
set_target_properties(chord_test PROPERTIES VERSION "${FULL_VERSION}" SOVERSION "${MAJOR_VERSION}")

# set the RPATH if on OS X
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(chord_test PROPERTIES MACOSX_RPATH TRUE)
endif()

# set the public header include path differently on the target depending on the interface
target_include_directories(chord_test PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

if(${IS_DEBUG_BUILD})
    target_compile_definitions(chord_test PRIVATE
        "CHORD_AGENT_EXECUTABLE=\"${CHORD_AGENT_DIST_PATH}\"")
else()
    target_compile_definitions(chord_test PRIVATE
        "CHORD_AGENT_EXECUTABLE=\"${CHORD_AGENT_INSTALL_PATH}\"")
endif()

target_link_libraries(chord_test
    PUBLIC
    chord::chord_sandbox
    lyric::lyric_build
    lyric::lyric_test
    tempo::tempo_utils
    gtest::gtest
    )

# dependencies needed for sandbox testing
add_dependencies(chord_test chord-agent chord-local-machine)

# create the workspace config file used for unit testing
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tester.config.in ${TESTER_CONFIG_PATH} @ONLY)
target_compile_definitions(chord_test PRIVATE
    "TESTER_CONFIG_PATH=\"${TESTER_CONFIG_PATH}\""
    )

# install targets
install(TARGETS chord_test EXPORT chord-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/chord_test
    )
