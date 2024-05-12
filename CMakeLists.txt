cmake_minimum_required(VERSION 3.20)

project(chord)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(IS_DEBUG_BUILD ON)
else()
    set(IS_DEBUG_BUILD OFF)
endif()

#######################################################################
#
# declare options
#
#######################################################################

set(RUNTIME_DISTRIBUTION_ROOT "${CMAKE_INSTALL_PREFIX}"
    CACHE STRING
    "The runtime distribution root directory. Defaults to CMAKE_INSTALL_PREFIX.")
message(STATUS "runtime distribution root is ${RUNTIME_DISTRIBUTION_ROOT}")

option(USE_SANITIZER "Enable sanitizer library" OFF)

set(SANITIZER ""
    CACHE STRING
    "one of 'address', 'thread', 'memory', 'ub', or 'leak'")

set(SANITIZER_IGNORELIST ""
    CACHE PATH
    "Instruct sanitizer to ignore symbols specified in ignorelist")

option(USE_PROFILER "Enable gperftools profiler" OFF)

#######################################################################
#
# configure compiler and linker
#
#######################################################################

# configure common compiler flags
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wall -Werror -Wno-deprecated-declarations")

# by default do not specify -O
set (OPTIMIZATION_LEVEL "")

# enable one of the sanitizer libraries if specified
if (USE_SANITIZER)
    if ("${SANITIZER}" STREQUAL "address")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")
        set (OPTIMIZATION_LEVEL "-O1")
        message(STATUS "enabling address sanitizer library")
    elseif ("${SANITIZER}" STREQUAL "thread")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
        set (OPTIMIZATION_LEVEL "-O1")
        message(STATUS "enabling thread sanitizer library")
    elseif ("${SANITIZER}" STREQUAL "memory")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=memory")
        set (OPTIMIZATION_LEVEL "-O1")
        message(STATUS "enabling memory sanitizer library")
    elseif ("${SANITIZER}" STREQUAL "ub")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
        set (OPTIMIZATION_LEVEL "-O1")
        message(STATUS "enabling ub sanitizer library")
    elseif ("${SANITIZER}" STREQUAL "leak")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak")
        set (OPTIMIZATION_LEVEL "-O1")
        message(STATUS "enabling leak sanitizer library")
    else()
        message(WARNING "skipping unknown sanitizer ${SANITIZER}")
        set (SANITIZER "")
    endif ("${SANITIZER}" STREQUAL "address")
    # if a sanitizer is enabled and an ignorelist is specified, then add it to the cxx flags
    if (SANITIZER)
        if (SANITIZER_IGNORELIST)
            set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize-ignorelist=${SANITIZER_IGNORELIST}")
            message(STATUS "using sanitizer ignorelist ${SANITIZER_IGNORELIST}")
        endif (SANITIZER_IGNORELIST)
    endif (SANITIZER)
else()
    message(STATUS "sanitizer not enabled")
endif (USE_SANITIZER)

#
if (NOT OPTIMIZATION_LEVEL)
    set(OPTIMIZATION_LEVEL "-O0 -fno-eliminate-unused-debug-types")
endif()

# set the optimization level
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPTIMIZATION_LEVEL}")

# set rpath for executable targets based on relative path from the executable
if (APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
elseif(WIN32)
    # do nothing
else()
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()

#######################################################################
#
# find required and optional dependencies
#
#######################################################################

# find required Tempo dependency
find_package(tempo REQUIRED)

# find required Lyric dependency
find_package(lyric REQUIRED)

# find required Zuri dependency
find_package(zuri REQUIRED)

# find required Abseil dependency
find_package(absl REQUIRED)

# find required grpc dependency
find_package(grpc REQUIRED)

# find required protobuf dependency
find_package(protobuf REQUIRED)

# find required Boost dependency
find_package(Boost REQUIRED)

# find required Openssl dependency
find_package(openssl REQUIRED)

# find required Fmt dependency
find_package(fmt REQUIRED)

# find required ICU dependency
find_package(icu REQUIRED)

# find required RocksDB dependency
find_package(rocksdb REQUIRED)

# find required Libuv dependency
find_package(uv REQUIRED)

# find required google test dependency
find_package(gtest REQUIRED)

# link to gperftools profiler library if specified
set (PROFILER_LIBRARIES "")
if (${USE_PROFILER})
    set (PROFILER_LIBRARIES "-lprofiler")
    message(STATUS "enabling gperftools profiler")
else()
    message(STATUS "gperftools profiler not enabled")
endif(${USE_PROFILER})

# include needed CMake features
include(CMakePackageConfigHelpers)
include(CTest)
include(GNUInstallDirs)
include(GoogleTest)

#######################################################################
#
# configure tools and paths
#
#######################################################################

# define path to protoc
message(STATUS "protoc command is ${PROTOBUF_PROTOC}")

# define path to grpc_cpp_plugin
message(STATUS "grpc_cpp_plugin command is ${GRPC_CPP_PLUGIN}")

#######################################################################
#
# define build constants
#
#######################################################################

set(FULL_PLATFORM "${CMAKE_SYSTEM_NAME}.${CMAKE_SYSTEM_VERSION}-${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}")
message(STATUS "full platform is ${FULL_PLATFORM}")

# define individual version components and version string
string(REGEX REPLACE "([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1"
    MAJOR_VERSION
    "${CHORD_PACKAGE_VERSION}")
string(REGEX REPLACE "[0-9]+\\.([0-9]+)\\.[0-9]+" "\\1"
    MINOR_VERSION
    "${CHORD_PACKAGE_VERSION}")
string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1"
    PATCH_VERSION
    "${CHORD_PACKAGE_VERSION}")
set(FULL_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")
message(STATUS "version is ${FULL_VERSION}")

set(CHORD_LOCAL_MACHINE_DIST_PATH           "${CMAKE_BINARY_DIR}/bin/chord_local_machine/chord-local-machine")
set(CHORD_LOCAL_MACHINE_INSTALL_PATH        "${CMAKE_INSTALL_BINDIR}/bin/chord-local-machine")
set(CHORD_AGENT_DIST_PATH                   "${CMAKE_BINARY_DIR}/bin/chord_agent/chord-agent")
set(CHORD_AGENT_INSTALL_PATH                "${CMAKE_INSTALL_BINDIR}/bin/chord-agent")

# used by chord_test to initialize sandbox tester
set(TESTER_CONFIG_PATH                      "${CMAKE_BINARY_DIR}/chord_test/tester.config")

# assembly locations for each example
set(EXAMPLES_DEMO_PACKAGE       "file://${CMAKE_BINARY_DIR}/share/examples/demo/examples_demo-${FULL_VERSION}.zpk")

# empty cpp file is used for targets which don't have any source files
set(EMPTY_CPP ${CMAKE_CURRENT_BINARY_DIR}/empty.cpp)

#######################################################################
#
# add library targets from subdirectories
#
#######################################################################

add_subdirectory(bin)
add_subdirectory(lib)
add_subdirectory(share)

#######################################################################
#
# declare the package test suite target
#
#######################################################################

add_custom_command(OUTPUT ${EMPTY_CPP} COMMAND cmake -E touch ${EMPTY_CPP})

add_executable(chord_testsuite ${EMPTY_CPP})
target_link_libraries(chord_testsuite
    PUBLIC gtest::gtest
    PRIVATE
    ChordLocalMachineTestSuite
    ChordSandboxTestSuite
)
gtest_discover_tests(chord_testsuite)

#######################################################################
#
# create cmake package config
#
#######################################################################

configure_package_config_file(
    cmake/chord-config.cmake.in
    chord-config.cmake
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/chord"
    PATH_VARS
      CMAKE_INSTALL_BINDIR
      CMAKE_INSTALL_INCLUDEDIR
      CMAKE_INSTALL_LIBDIR
      CMAKE_INSTALL_DATADIR
    )

write_basic_package_version_file(chord-config-version.cmake
    VERSION "${FULL_VERSION}"
    COMPATIBILITY SameMajorVersion
    )

install(EXPORT chord-targets
    COMPONENT Devel
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/chord"
    NAMESPACE chord::
    )

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/chord-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/chord-config-version.cmake"
    COMPONENT Devel
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/chord"
    )
