#ifndef CHORD_TEST_CHORD_SANDBOX_TESTER_H
#define CHORD_TEST_CHORD_SANDBOX_TESTER_H

#include <filesystem>

#include <absl/container/flat_hash_map.h>

#include <chord_common/abstract_protocol_handler.h>
#include <chord_sandbox/chord_isolate.h>
#include <chord_sandbox/run_protocol_plug.h>
#include <lyric_build/lyric_builder.h>
#include <lyric_runtime/bytecode_interpreter.h>
#include <lyric_runtime/interpreter_state.h>
#include <lyric_test/test_runner.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_security/ecc_private_key_generator.h>
#include <zuri_distributor/package_cache.h>
#include <zuri_distributor/package_cache_loader.h>
#include <zuri_packager/package_specifier.h>

#include "test_result.h"
#include "test_run.h"

namespace chord_test {

    struct SandboxTesterOptions {
        std::filesystem::path testRootDirectory = {};
        bool useInMemoryCache = true;
        bool isTemporary = true;
        bool keepBuildOnUnexpectedResult = true;
        std::shared_ptr<lyric_build::TaskRegistry> taskRegistry = {};
        std::shared_ptr<lyric_runtime::AbstractLoader> bootstrapLoader = {};
        std::shared_ptr<lyric_runtime::AbstractLoader> fallbackLoader = {};
        lyric_build::TaskSettings taskSettings = {};
        std::filesystem::path existingPackageCache = {};
        std::vector<std::filesystem::path> localPackages;
        std::vector<std::string> mainArguments = {};
        std::vector<chord_sandbox::RequestedPortAndHandler> protocolPlugs;
    };

    class ChordSandboxTester {

    public:
        explicit ChordSandboxTester(const SandboxTesterOptions &options);

        tempo_utils::Status configure();

        const lyric_test::TestRunner *getRunner() const;

        tempo_utils::Result<SpawnMachine> spawnProgramInSandbox(const tempo_utils::Url &mainLocation);

        tempo_utils::Result<SpawnMachine> spawnProgramInSandbox(const std::filesystem::path &packagePath);

        tempo_utils::Result<RunMachine> runModuleInSandbox(
            const std::string &code,
            const std::filesystem::path &modulePath = {},
            const std::filesystem::path &baseDir = {});

        static tempo_utils::Result<SpawnMachine> spawnSingleProgramInSandbox(
            const tempo_utils::Url &mainLocation,
            const SandboxTesterOptions &options = {});

        static tempo_utils::Result<SpawnMachine> spawnSingleProgramInSandbox(
            const std::filesystem::path &packagePath,
            const SandboxTesterOptions &options = {});

        static tempo_utils::Result<RunMachine> runSingleModuleInSandbox(
            const std::string &code,
            const SandboxTesterOptions &options = {});

    private:
        SandboxTesterOptions m_options;
        std::shared_ptr<zuri_distributor::PackageCache> m_packageCache;
        std::shared_ptr<zuri_distributor::PackageCacheLoader> m_packageCacheLoader;
        std::shared_ptr<chord_sandbox::ChordIsolate> m_isolate;
        std::shared_ptr<lyric_test::TestRunner> m_runner;
        std::string m_domain;
        tempo_security::ECCPrivateKeyGenerator m_eccKeygen;
        tempo_security::CertificateKeyPair m_caKeyPair;

        tempo_utils::Result<zuri_packager::PackageSpecifier> makePackage(
            const lyric_build::TargetComputation &targetComputation);
    };
}

#endif // CHORD_TEST_CHORD_SANDBOX_TESTER_H
