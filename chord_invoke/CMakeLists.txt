
# build chord_invoke as a shared library
add_library(chord_invoke SHARED)
add_library(chord::chord_invoke ALIAS chord_invoke)

add_custom_command (
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke/invoke_service.grpc.pb.cc
      ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke/invoke_service.grpc.pb.h
    COMMAND
      cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke
    COMMAND
      ${PROTOBUF_PROTOC} -I${CMAKE_CURRENT_SOURCE_DIR}/share
      --grpc_out=generate_mock_code=true:${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke
      --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
      ${CMAKE_CURRENT_SOURCE_DIR}/share/invoke_service.proto
    DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/share/invoke_service.proto
)

add_custom_command (
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke/invoke_service.pb.cc
      ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke/invoke_service.pb.h
    COMMAND
      cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke
    COMMAND
      ${PROTOBUF_PROTOC} -I${CMAKE_CURRENT_SOURCE_DIR}/share
      --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke
      ${CMAKE_CURRENT_SOURCE_DIR}/share/invoke_service.proto
    DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/share/invoke_service.proto
)

set(CHORD_INVOKE_INCLUDES
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke/invoke_service.grpc.pb.h
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke/invoke_service.pb.h
)
set_target_properties(chord_invoke PROPERTIES PUBLIC_HEADER "${CHORD_INVOKE_INCLUDES}")

target_sources(chord_invoke PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke/invoke_service.grpc.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke/invoke_service.grpc.pb.h
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke/invoke_service.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_invoke/invoke_service.pb.h
)

# set the library version
set_target_properties(chord_invoke PROPERTIES VERSION "${FULL_VERSION}" SOVERSION "${MAJOR_VERSION}")

# set the RPATH if on OS X
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(chord_invoke PROPERTIES MACOSX_RPATH TRUE)
endif()

# set the public header include path differently on the target depending on the interface
target_include_directories(chord_invoke PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(chord_invoke PUBLIC gRPC::grpc++)

# install targets
install(TARGETS chord_invoke EXPORT chord-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/chord_invoke
    )

# install data
install(FILES share/invoke_service.proto DESTINATION ${CMAKE_INSTALL_DATADIR}/chord_invoke)

# add testing subdirectory
#add_subdirectory(test)
