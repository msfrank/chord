
set(SOURCE_DIRECTORY        "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PACKAGE_DIRECTORY       "${CMAKE_CURRENT_BINARY_DIR}")
set(WORKSPACE_CONFIG_FILE   "${CMAKE_CURRENT_BINARY_DIR}/workspace.config")
set(HELLOWORLD_PACKAGE_URL  "dev.zuri.pkg://demo-${FULL_VERSION}@examples/helloworld")

configure_file(workspace.config.in ${CMAKE_CURRENT_BINARY_DIR}/workspace.config @ONLY)

# build the demo example package

add_custom_command (
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/install/examples_demo-${FULL_VERSION}.zpk
    COMMAND
      zuri::zuri-build -v
        -W ${CMAKE_CURRENT_BINARY_DIR}
        -B ${CMAKE_CURRENT_BINARY_DIR}
        -I ${PACKAGE_DIRECTORY}
        package:
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS
      zuri::zuri-build
      ${CMAKE_CURRENT_BINARY_DIR}/workspace.config
)

add_custom_target(examples-demo DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/install/examples_demo-${FULL_VERSION}.zpk)

add_subdirectory(test)