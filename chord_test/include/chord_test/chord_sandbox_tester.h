#ifndef CHORD_TEST_LYRIC_SANDBOX_TESTER_H
#define CHORD_TEST_LYRIC_SANDBOX_TESTER_H

#include <filesystem>

#include <absl/container/flat_hash_map.h>

#include <chord_protocol/abstract_protocol_handler.h>
#include <chord_sandbox/run_protocol_plug.h>
#include <chord_test/test_result.h>
#include <lyric_build/lyric_builder.h>
#include <lyric_runtime/bytecode_interpreter.h>
#include <lyric_runtime/interpreter_state.h>
#include <lyric_test/test_runner.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_security/ecc_private_key_generator.h>

namespace chord_test {

    struct SandboxTesterOptions {
        std::filesystem::path testRootDirectory;
        bool useInMemoryCache = true;
        bool isTemporary = true;
        bool keepBuildOnUnexpectedResult = true;
        std::string preludeLocation;
        absl::flat_hash_map<std::string,std::string> packageMap;
        tempo_config::ConfigMap buildConfig;
        tempo_config::ConfigMap buildVendorConfig;
        absl::flat_hash_set<chord_protocol::RequestedPort> requestedPorts;
        absl::flat_hash_map<
            tempo_utils::Url,
            std::shared_ptr<chord_protocol::AbstractProtocolHandler>> protocolPlugs;
        chord_sandbox::RunProtocolCallback runProtocolCallback = nullptr;
        void *runProtocolCallbackData = nullptr;
    };

    class ChordSandboxTester {

    public:
        explicit ChordSandboxTester(const SandboxTesterOptions &options);

        tempo_utils::Status configure();

        const lyric_test::TestRunner *getRunner() const;

        tempo_utils::Result<lyric_test::RunModule> runModuleInSandbox(
            const std::string &code,
            const std::filesystem::path &path = {});

        static tempo_utils::Result<lyric_test::RunModule> runSingleModuleInSandbox(
            const std::string &code,
            const SandboxTesterOptions &options = {});

    private:
        SandboxTesterOptions m_options;
        std::shared_ptr<lyric_test::TestRunner> m_runner;
        std::string m_domain;
        tempo_security::ECCPrivateKeyGenerator m_eccKeygen;
        tempo_security::CertificateKeyPair m_caKeyPair;
    };
}

#endif // CHORD_TEST_LYRIC_SANDBOX_TESTER_H
