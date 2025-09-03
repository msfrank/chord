
set(PACKAGE_BUILD_DIR               "${CHORD_BUILD_PACKAGES_DIR}/${ZURI_NET_PACKAGE_ID}")
set(PACKAGE_INSTALL_DIR             "${PACKAGES_DIR_PREFIX}/${ZURI_NET_PACKAGE_ID}")

# create the workspace config file used for unit testing
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tester.config.in ${TESTER_CONFIG_PATH} @ONLY)

add_subdirectory(http)

add_custom_target(ZuriNet DEPENDS
    ZuriNetHttp
    )