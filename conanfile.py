from os.path import join

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load

class Chord(ConanFile):
    name = 'chord'

    settings = 'os', 'compiler', 'build_type', 'arch'
    options = {
        'runtime_distribution_root': ['ANY', None],
        'enable_sanitizer': [True, False, None],
        'sanitizer': ['address', 'thread', 'memory', 'ub', 'leak', None],
        'enable_profiler': [True, False, None],
        }
    default_options = {
        'runtime_distribution_root': None,
        'enable_sanitizer': None,
        'sanitizer': None,
        'enable_profiler': None,
        }

    exports = ('meta/*',)

    exports_sources = (
        'CMakeLists.txt',
        'bin/*',
        'cmake/*',
        'lib/*',
        'meta/*',
        'pkg/*',
        'share/*',
        )

    requires = (
        'lyric/0.0.1',
        'tempo/0.0.1',
        'zuri/0.0.1',
        # requirements from timbre
        'absl/20250127.1@timbre',
        'boost/1.88.0@timbre',
        'curl/8.15.0@timbre',
        'fmt/12.0.0@timbre',
        'flatbuffers/25.2.10@timbre',
        'grpc/1.74.1@timbre',
        'gtest/1.17.0@timbre',
        'openssl/3.5.2@timbre',
        'protobuf/32.0@timbre',
        'rocksdb/10.4.2@timbre',
        'sqlite/3.49.2@timbre',
        'uv/1.51.0@timbre',
        )

    def _get_meta(self, key):
        return load(self, join(self.recipe_folder, "meta", key))

    def set_version(self):
        self.version = self._get_meta('version')
        self.license = self._get_meta('license')
        self.url = self._get_meta('url')
        self.description = self._get_meta('description')

    def validate(self):
        check_min_cppstd(self, "20")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        protobuf = self.dependencies['protobuf'].buildenv_info.vars(self)
        grpc = self.dependencies['grpc'].buildenv_info.vars(self)

        tc = CMakeToolchain(self)
        tc.cache_variables['CHORD_PACKAGE_VERSION'] = self.version
        tc.cache_variables['PROTOBUF_PROTOC'] = protobuf.get('PROTOBUF_PROTOC')
        tc.cache_variables['GRPC_CPP_PLUGIN'] = grpc.get('GRPC_CPP_PLUGIN')

        if self.options.runtime_distribution_root:
            tc.cache_variables['RUNTIME_DISTRIBUTION_ROOT'] = self.options.runtime_distribution_root
        if self.options.enable_sanitizer:
            tc.cache_variables['ENABLE_SANITIZER'] = self.options.enable_sanitizer
        if self.options.sanitizer:
            tc.cache_variables['SANITIZER'] = self.options.sanitizer
        if self.options.enable_profiler:
            tc.cache_variables['ENABLE_PROFILER'] = self.options.enable_profiler

        tc.generate()
        deps = CMakeDeps(self)
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
