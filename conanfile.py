from os.path import join

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy

class Chord(ConanFile):
    name = 'chord'
    version = '0.0.1'
    license = 'BSD-3-Clause, AGPL-3.0-or-later'
    url = 'https://github.com/msfrank/chord'
    description = ''

    settings = 'os', 'compiler', 'build_type', 'arch'
    options = {'shared': [True, False], 'compiler.cppstd': ['17', '20'], 'build_type': ['Debug', 'Release']}
    default_options = {'shared': True, 'compiler.cppstd': '20', 'build_type': 'Debug'}

    exports_sources = (
        'CMakeLists.txt',
        'bin/*',
        'cmake/*',
        'lib/*',
        'pkg/*',
        'share/*',
        )

    requires = (
        'lyric/0.0.1',
        'tempo/0.0.1',
        'zuri/0.0.1',
        # requirements from timbre
        'absl/20230802.1@timbre',
        'boost/1.84.0@timbre',
        'curl/8.5.0@timbre',
        'fmt/9.1.0@timbre',
        'flatbuffers/23.5.26@timbre',
        'grpc/1.62.0@timbre',
        'gtest/1.14.0@timbre',
        'icu/74.1@timbre',
        'openssl/3.2.0@timbre',
        'protobuf/25.3@timbre',
        'rocksdb/8.5.3@timbre',
        'uv/1.44.1@timbre',
        )

    def validate(self):
        check_min_cppstd(self, "20")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        protobuf = self.dependencies['protobuf'].buildenv_info.vars(self)
        grpc = self.dependencies['grpc'].buildenv_info.vars(self)

        tc = CMakeToolchain(self)
        tc.variables['CHORD_PACKAGE_VERSION'] = self.version
        tc.variables['PROTOBUF_PROTOC'] = protobuf.get('PROTOBUF_PROTOC')
        tc.variables['GRPC_CPP_PLUGIN'] = grpc.get('GRPC_CPP_PLUGIN')
        tc.generate()

        deps = CMakeDeps(self)
        deps.set_property("openssl::crypto", "cmake_target_name", "OpenSSL::Crypto")
        deps.set_property("openssl::ssl", "cmake_target_name", "OpenSSL::SSL")
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs.append(join("lib", "cmake", "chord"))
