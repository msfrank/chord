#include <gtest/gtest.h>

#include <chord_sandbox/chord_isolate.h>
#include <chord_test/chord_sandbox_tester.h>
#include <chord_test/matchers.h>
#include <tempo_config/program_config.h>
#include <tempo_security/ecc_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_test/tempo_test.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/tempdir_maker.h>

class ChordIsolate : public ::testing::Test {
protected:
    chord_test::SandboxTesterOptions options;

    void SetUp() override {
        tempo_config::ProgramConfigOptions zuriOptions;
        zuriOptions.distConfigDirectoryPath = ZURI_INSTALL_CONFIG_DIR;
        zuriOptions.distVendorConfigDirectoryPath = ZURI_INSTALL_VENDOR_CONFIG_DIR;
        zuriOptions.toolLocator = {"zuri-build"};
        auto loadProgramResult = tempo_config::ProgramConfig::load(zuriOptions);
        ASSERT_TRUE (loadProgramResult.isResult());
        auto config = loadProgramResult.getResult();
        options.isTemporary = false;
        options.buildConfig = config->getToolConfig();
        options.buildVendorConfig = config->getVendorConfig();
    }
};

TEST_F(ChordIsolate, InitializeAndShutdown)
{
    std::string organization("test");
    auto organizationalUnit = tempo_utils::generate_name("XXXXXXXX");
    auto caCommonName = absl::StrCat("ca.", organizationalUnit, ".", organization);
    auto agentPrefix = tempo_utils::generate_name("agent-XXXXXXXX");
    auto agentServerName = absl::StrCat(agentPrefix, ".", organizationalUnit, ".", organization);

    tempo_security::ECCPrivateKeyGenerator keygen(NID_X9_62_prime256v1);

    tempo_utils::TempdirMaker tempdirMaker("tester.XXXXXXXX");
    ASSERT_TRUE (tempdirMaker.isValid());
    auto testerDirectory = tempdirMaker.getTempdir();

    auto generateCAKeyPairResult = tempo_security::generate_self_signed_ca_key_pair(keygen,
        organization, organizationalUnit, caCommonName,
        1, std::chrono::seconds{60}, -1,
        testerDirectory, "ca");
    ASSERT_TRUE (generateCAKeyPairResult.isResult());
    auto caKeyPair = generateCAKeyPairResult.getResult();

    auto generateAgentKeyPairResult = tempo_security::generate_key_pair(caKeyPair, keygen,
        organization, organizationalUnit, agentServerName,
        1, std::chrono::seconds{60},
        testerDirectory, agentPrefix);
    ASSERT_TRUE (generateAgentKeyPairResult.isResult());
    auto agentKeyPair = generateAgentKeyPairResult.getResult();

    // construct the sandbox
    chord_sandbox::SandboxOptions options;
    options.discoveryPolicy = chord_sandbox::AgentDiscoveryPolicy::ALWAYS_SPAWN;
    options.endpointTransport = chord_protocol::TransportType::Unix;
    options.agentPath = std::filesystem::path(CHORD_AGENT_EXECUTABLE);
    options.runDirectory = testerDirectory;
    options.caKeyPair = caKeyPair;
    options.agentKeyPair = agentKeyPair;
    options.pemRootCABundleFile = caKeyPair.getPemCertificateFile();
    options.agentServerName = agentServerName;

    // initialize the sandbox
    chord_sandbox::ChordIsolate isolate(options);
    auto status = isolate.initialize();
    ASSERT_TRUE (status.isOk());

    // shut down the sandbox
    status = isolate.shutdown();
    ASSERT_TRUE (status.isOk());
}

TEST_F(ChordIsolate, SpawnRemoteMachine)
{
    auto result = chord_test::ChordSandboxTester::runSingleModuleInSandbox(R"(
        return 42
    )", options);

    ASSERT_THAT (result, tempo_test::ContainsResult(RunMachine(tempo_utils::StatusCode::kOk)));
}
