
# create the workspace config file used to build package
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/workspace.config.in ${CMAKE_CURRENT_BINARY_DIR}/workspace.config @ONLY)

# build test1 package
add_custom_command (
    OUTPUT ${TEST1_ZPK}
    COMMAND
      zuri::zuri-build -v -W ${CMAKE_CURRENT_SOURCE_DIR} -B ${CMAKE_CURRENT_BINARY_DIR} -I ${CMAKE_CURRENT_BINARY_DIR}
      --workspace-config-file ${CMAKE_CURRENT_BINARY_DIR}/workspace.config
      --distribution-root ${ZURI_INSTALL_DISTRIBUTION_ROOT}
      "test1"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS zuri::zuri-build
)
add_custom_target(chord-agent-test1 DEPENDS ${TEST1_ZPK})


add_custom_target(chord-agent-test-data DEPENDS
    chord-agent-test1
    )
