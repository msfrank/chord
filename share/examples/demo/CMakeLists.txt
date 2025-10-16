
# create the workspace config file used to build package
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/workspace.config.in ${CMAKE_CURRENT_BINARY_DIR}/workspace.config @ONLY)

# build the demo package
add_custom_command (
    OUTPUT
      ${CHORD_DEMO_PACKAGE_PATH}
    COMMAND
      zuri::zuri-build -v
      -W ${CMAKE_CURRENT_SOURCE_DIR}
      --workspace-config-file ${CMAKE_CURRENT_BINARY_DIR}/workspace.config
      -B ${CMAKE_CURRENT_BINARY_DIR}
      -I ${CMAKE_BINARY_DIR}
      --distribution-root ${ZURI_INSTALL_DISTRIBUTION_ROOT}
      "demo"
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
      zuri::zuri-build
      ${CMAKE_CURRENT_BINARY_DIR}/workspace.config
    )

add_custom_target(examples-demo DEPENDS ${CHORD_DEMO_PACKAGE_PATH})

add_subdirectory(test)