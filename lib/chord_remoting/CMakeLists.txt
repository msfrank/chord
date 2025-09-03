
# do not run moc automatically
set(CMAKE_AUTOMOC OFF)

# build chord_remoting as a shared library
add_library(chord_remoting SHARED)
add_library(chord::chord_remoting ALIAS chord_remoting)

add_custom_command (
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting/remoting_service.grpc.pb.cc
      ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting/remoting_service.grpc.pb.h
    COMMAND
      cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting
    COMMAND
      ${PROTOBUF_PROTOC} -I${CMAKE_CURRENT_SOURCE_DIR}/share
      --grpc_out=generate_mock_code=true:${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting
      --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
      ${CMAKE_CURRENT_SOURCE_DIR}/share/remoting_service.proto
    DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/share/remoting_service.proto
)

add_custom_command (
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting/remoting_service.pb.cc
      ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting/remoting_service.pb.h
    COMMAND
      cmake -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting
    COMMAND
      ${PROTOBUF_PROTOC} -I${CMAKE_CURRENT_SOURCE_DIR}/share
      --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting
      ${CMAKE_CURRENT_SOURCE_DIR}/share/remoting_service.proto
    DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/share/remoting_service.proto
)

set(CHORD_REMOTING_INCLUDES
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting/remoting_service.grpc.pb.h
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting/remoting_service.pb.h
)
set_target_properties(chord_remoting PROPERTIES PUBLIC_HEADER "${CHORD_REMOTING_INCLUDES}")

target_sources(chord_remoting PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting/remoting_service.grpc.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting/remoting_service.grpc.pb.h
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting/remoting_service.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/generated/chord_remoting/remoting_service.pb.h
)

# set the library version
set_target_properties(chord_remoting PROPERTIES VERSION "${FULL_VERSION}" SOVERSION "${MAJOR_VERSION}")

# set the RPATH if on OS X
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set_target_properties(chord_remoting PROPERTIES MACOSX_RPATH TRUE)
endif()

# set the public header include path differently on the target depending on the interface
target_include_directories(chord_remoting PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(chord_remoting PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CHORD_BUILD_LIB_DIR}
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${LIB_RPATH}
)

target_link_libraries(chord_remoting PUBLIC gRPC::grpc++)

# install targets
install(TARGETS chord_remoting EXPORT chord-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/chord_remoting
)

# install data
install(FILES share/remoting_service.proto DESTINATION ${CMAKE_INSTALL_DATADIR}/chord_remoting)

# add testing subdirectory
#add_subdirectory(test)
