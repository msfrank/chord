
set(LIB_NAME "http")

set(ZURI_NET_HTTP_ASSEMBLY_NAME         "${LIB_NAME}.lyo")
set(ZURI_NET_HTTP_ASSEMBLY_PATH         "${PACKAGE_BUILD_DIR}/${ZURI_NET_HTTP_ASSEMBLY_NAME}")
set(ZURI_NET_HTTP_PLUGIN_BASENAME       "${LIB_NAME}.${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(ZURI_NET_HTTP_PLUGIN_NAME           "${ZURI_NET_HTTP_PLUGIN_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
set(ZURI_NET_HTTP_PLUGIN_PATH           "${PACKAGE_BUILD_DIR}/${ZURI_NET_HTTP_PLUGIN_NAME}")

set(ZURI_NET_HTTP_LOCATION              "${ZURI_NET_PACKAGE_URL}/${LIB_NAME}")

# create the platform configuration header file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lib_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/zuri_net_http/config.h
    @ONLY
    )

add_executable(zuri-net-http-builder
    assembly/compile_manager.cpp
    assembly/compile_manager.h
    assembly/compile_response.cpp
    assembly/compile_response.h
    assembly/lib_builder.cpp
    include/zuri_net_http/lib_types.h
    ${CMAKE_CURRENT_BINARY_DIR}/include/zuri_net_http/config.h
    )

target_include_directories(zuri-net-http-builder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    )

target_compile_definitions(zuri-net-http-builder PRIVATE
    "ZURI_STD_PACKAGE_URL=\"${ZURI_STD_PACKAGE_URL}\""
    "ZURI_INSTALL_PACKAGES_DIR=\"${ZURI_INSTALL_PACKAGES_DIR}\""
    "CHORD_BUILD_PACKAGES_DIR=\"${CHORD_BUILD_PACKAGES_DIR}\""
    "DEFAULT_DESTINATION_PATH=\"${ZURI_NET_HTTP_ASSEMBLY_PATH}\""
    )

target_link_libraries(zuri-net-http-builder
    lyric::lyric_build
    tempo::tempo_command
    tempo::tempo_utils
    )

# build assembly file
add_custom_command (
    OUTPUT ${ZURI_NET_HTTP_ASSEMBLY_PATH}
    COMMAND zuri-net-http-builder ${ZURI_NET_HTTP_ASSEMBLY_PATH}
    COMMENT "generating ${ZURI_NET_HTTP_ASSEMBLY_NAME}"
    DEPENDS zuri-net-http-builder
    )
add_custom_target(zuri-net-http-assembly DEPENDS ${ZURI_NET_HTTP_ASSEMBLY_PATH})

# build string plugin as a shared library
add_library(zuri-net-http-plugin SHARED
    plugin/curl_headers.cpp
    plugin/curl_headers.h
    plugin/curl_utils.cpp
    plugin/curl_utils.h
    plugin/manager_ref.cpp
    plugin/manager_ref.h
    plugin/plugin.cpp
    plugin/plugin.h
    include/zuri_net_http/lib_types.h
    ${CMAKE_CURRENT_BINARY_DIR}/include/zuri_net_http/config.h
    )

#
set_target_properties(zuri-net-http-plugin PROPERTIES
    LIBRARY_OUTPUT_NAME ${ZURI_NET_HTTP_PLUGIN_BASENAME}
    LIBRARY_OUTPUT_DIRECTORY ${PACKAGE_BUILD_DIR}
    PREFIX ""
    )

# set the RPATH if on OS X
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(zuri-net-http-plugin PROPERTIES MACOSX_RPATH TRUE)
endif()

target_include_directories(zuri-net-http-plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    )

target_link_libraries(zuri-net-http-plugin PRIVATE
    lyric::lyric_runtime
    CURL::libcurl_shared
    )

add_custom_target(ZuriNetHttp DEPENDS zuri-net-http-assembly zuri-net-http-plugin)

# install targets
install(TARGETS zuri-net-http-plugin LIBRARY DESTINATION ${PACKAGE_INSTALL_DIR})
install(FILES ${ZURI_NET_HTTP_ASSEMBLY_PATH} DESTINATION ${PACKAGE_INSTALL_DIR})

# add testing subdirectory
add_subdirectory(test)
