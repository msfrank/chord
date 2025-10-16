cmake_minimum_required(VERSION 3.27)

# read project metadata
file(READ ${CMAKE_SOURCE_DIR}/meta/version CHORD_PROJECT_VERSION)
string(STRIP ${CHORD_PROJECT_VERSION} CHORD_PROJECT_VERSION)
file(READ ${CMAKE_SOURCE_DIR}/meta/url CHORD_PROJECT_URL)
string(STRIP ${CHORD_PROJECT_URL} CHORD_PROJECT_URL)
file(READ ${CMAKE_SOURCE_DIR}/meta/description CHORD_PROJECT_DESCRIPTION)
string(STRIP ${CHORD_PROJECT_DESCRIPTION} CHORD_PROJECT_DESCRIPTION)
file(READ ${CMAKE_SOURCE_DIR}/meta/license CHORD_PROJECT_LICENSE)
string(STRIP ${CHORD_PROJECT_LICENSE} CHORD_PROJECT_LICENSE)

project(chord
    VERSION ${CHORD_PROJECT_VERSION}
    HOMEPAGE_URL ${CHORD_PROJECT_URL}
    DESCRIPTION ${CHORD_PROJECT_DESCRIPTION}
)

# if build type was not explicitly defined then default to Debug
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "build type not specified so defaulting to Debug" FORCE)
endif()

# define IS_DEBUG_BUILD boolean to make it easier to check whether we are in debug mode
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(IS_DEBUG_BUILD ON)
else()
    set(IS_DEBUG_BUILD OFF)
endif()


#######################################################################
#
# enable/disable build targets
#
#######################################################################

option(DISABLE_LOCAL_BUILD "Disable local build" FALSE)
option(ENABLE_DOCKER_BUILD "Enable docker build" FALSE)

# if docker build is enabled then add targets from docker directory
if (ENABLE_DOCKER_BUILD)
    add_subdirectory(docker)
endif()

# if local build is disabled then don't process any further targets
if (DISABLE_LOCAL_BUILD)
    return()
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

option(ENABLE_SANITIZER "Enable sanitizer library." OFF)

set(SANITIZER ""
    CACHE STRING
    "The sanitizer to use. Must be one of 'address', 'thread', 'memory', 'ub', or 'leak'.")

set(SANITIZER_IGNORELIST ""
    CACHE PATH
    "Instruct sanitizer to ignore symbols specified in ignorelist.")

option(ENABLE_PROFILER "Enable gperftools profiler." OFF)


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

# define the rpath for executable and library targets
if (APPLE)
    set(BIN_RPATH "@executable_path/../lib")
    set(LIB_RPATH "@loader_path")
elseif(WIN32)
    # do nothing
else()
    set(BIN_RPATH "$ORIGIN/../lib")
    set(LIB_RPATH "$ORIGIN/")
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

# find required gRPC dependency
find_package(grpc REQUIRED)

# find required Protobuf dependency
find_package(protobuf REQUIRED)

# find required Curl dependency
find_package(CURL REQUIRED)

# find required Boost dependency
find_package(Boost REQUIRED)

# find required Openssl dependency
find_package(OpenSSL REQUIRED)

# find required Fmt dependency
find_package(fmt REQUIRED)

# find required ICU dependency
find_package(icu REQUIRED)

# find required RocksDB dependency
find_package(rocksdb REQUIRED)

# find required Sqlite dependency
find_package(sqlite REQUIRED)

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

# zuri package group
set(ZURI_PACKAGE_GROUP                          "dev.zuri")

# zuri net package
set(ZURI_NET_PACKAGE_NAME                       "net")
set(ZURI_NET_PACKAGE_VERSION                    "${PROJECT_VERSION}")
set(ZURI_NET_PACKAGE_ID                         "${ZURI_PACKAGE_GROUP}_${ZURI_NET_PACKAGE_NAME}-${ZURI_NET_PACKAGE_VERSION}")
set(ZURI_NET_PACKAGE_ZPK                        "${ZURI_NET_PACKAGE_ID}.zpk")
set(ZURI_NET_PACKAGE_AUTHORITY                  "${ZURI_NET_PACKAGE_NAME}-${ZURI_NET_PACKAGE_VERSION}@zuri.dev")
set(ZURI_NET_PACKAGE_URL                        "dev.zuri.pkg://${ZURI_NET_PACKAGE_AUTHORITY}")
set(ZURI_NET_PACKAGE_PATH                       "${CMAKE_BINARY_DIR}/${ZURI_NET_PACKAGE_ZPK}")

# alias the zuri prefix path variables
set(PACKAGES_DIR_PREFIX                         "${ZURI_PACKAGES_DIR_PREFIX}")
set(DOC_DIR_PREFIX                              "${ZURI_DOC_DIR_PREFIX}")
set(CONFIG_DIR_PREFIX                           "${ZURI_CONFIG_DIR_PREFIX}")
set(VENDOR_CONFIG_DIR_PREFIX                    "${ZURI_VENDOR_CONFIG_DIR_PREFIX}")

# install paths
set(CHORD_LOCAL_MACHINE_PATH_PREFIX             "${CMAKE_INSTALL_BINDIR}/chord-local-machine")
set(CHORD_AGENT_PATH_PREFIX                     "${CMAKE_INSTALL_BINDIR}/chord-agent")

# build paths
set(CHORD_BUILD_OUTPUT_ROOT                     "${CMAKE_BINARY_DIR}/output")
set(CHORD_BUILD_BIN_DIR                         "${CHORD_BUILD_OUTPUT_ROOT}/${CMAKE_INSTALL_BINDIR}")
set(CHORD_BUILD_LIB_DIR                         "${CHORD_BUILD_OUTPUT_ROOT}/${CMAKE_INSTALL_LIBDIR}")
set(CHORD_BUILD_PACKAGES_DIR                    "${CHORD_BUILD_OUTPUT_ROOT}/${PACKAGES_DIR_PREFIX}")
set(CHORD_BUILD_DOC_DIR                         "${CHORD_BUILD_OUTPUT_ROOT}/${DOC_DIR_PREFIX}")
set(CHORD_BUILD_CONFIG_DIR                      "${CHORD_BUILD_OUTPUT_ROOT}/${CONFIG_DIR_PREFIX}")
set(CHORD_BUILD_VENDOR_CONFIG_DIR               "${CHORD_BUILD_OUTPUT_ROOT}/${VENDOR_CONFIG_DIR_PREFIX}")
set(CHORD_BUILD_CHORD_LOCAL_MACHINE_PATH        "${CHORD_BUILD_OUTPUT_ROOT}/${CHORD_LOCAL_MACHINE_PATH_PREFIX}")
set(CHORD_BUILD_CHORD_AGENT_PATH                "${CHORD_BUILD_OUTPUT_ROOT}/${CHORD_AGENT_PATH_PREFIX}")

# runtime paths
set(CHORD_RUNTIME_DISTRIBUTION_ROOT             "${CMAKE_INSTALL_PREFIX}")
set(CHORD_RUNTIME_PACKAGES_DIR                  "${CMAKE_INSTALL_PREFIX}/${PACKAGES_DIR_PREFIX}")
set(CHORD_RUNTIME_CONFIG_DIR                    "${CMAKE_INSTALL_PREFIX}/${CONFIG_DIR_PREFIX}")
set(CHORD_RUNTIME_VENDOR_CONFIG_DIR             "${CMAKE_INSTALL_PREFIX}/${VENDOR_CONFIG_DIR_PREFIX}")
set(CHORD_RUNTIME_CHORD_LOCAL_MACHINE_PATH      "${CMAKE_INSTALL_PREFIX}/${CHORD_LOCAL_MACHINE_EXE_PREFIX}")
set(CHORD_RUNTIME_CHORD_AGENT_PATH              "${CMAKE_INSTALL_PREFIX}/${CHORD_AGENT_EXE_PREFIX}")

# used by chord_test to initialize sandbox tester
set(TESTER_CONFIG_PATH                          "${CMAKE_BINARY_DIR}/chord_test/tester.config")

# chord-examples package group
set(CHORD_EXAMPLES_PACKAGE_GROUP                "chord-examples")

# chord demo package
set(CHORD_DEMO_PACKAGE_NAME                     "demo")
set(CHORD_DEMO_PACKAGE_VERSION                  "${PROJECT_VERSION}")
set(CHORD_DEMO_PACKAGE_ID                       "${CHORD_EXAMPLES_PACKAGE_GROUP}_${CHORD_DEMO_PACKAGE_NAME}-${CHORD_DEMO_PACKAGE_VERSION}")
set(CHORD_DEMO_PACKAGE_ZPK                      "${CHORD_DEMO_PACKAGE_ID}.zpk")
set(CHORD_DEMO_PACKAGE_PATH                     "${CMAKE_BINARY_DIR}/${CHORD_DEMO_PACKAGE_ZPK}")

# empty cpp file is used for targets which don't have any source files
set(EMPTY_CPP ${CMAKE_CURRENT_BINARY_DIR}/empty.cpp)


#######################################################################
#
# add library targets from subdirectories
#
#######################################################################

add_subdirectory(bin)
add_subdirectory(lib)
add_subdirectory(pkg)
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
        CHORD_LOCAL_MACHINE_PATH_PREFIX
        CHORD_AGENT_PATH_PREFIX
        PACKAGES_DIR_PREFIX
        DOC_DIR_PREFIX
        CONFIG_DIR_PREFIX
        VENDOR_CONFIG_DIR_PREFIX
    )

write_basic_package_version_file(chord-config-version.cmake
    VERSION "${PROJECT_VERSION}"
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
