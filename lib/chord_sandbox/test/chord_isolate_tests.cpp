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
        options.isTemporary = false;
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

    tempo_security::CertificateKeyPair caKeyPair;
    TU_ASSIGN_OR_RAISE (caKeyPair, tempo_security::generate_self_signed_ca_key_pair(keygen,
        organization, organizationalUnit, caCommonName,
        1, std::chrono::seconds{60}, -1,
        testerDirectory, "ca"));

    tempo_security::CertificateKeyPair agentKeyPair;
    TU_ASSIGN_OR_RAISE (agentKeyPair, tempo_security::generate_key_pair(caKeyPair, keygen,
        organization, organizationalUnit, agentServerName,
        1, std::chrono::seconds{60},
        testerDirectory, agentPrefix));

    chord_tooling::SecurityConfig securityConfig;
    securityConfig.pemRootCABundleFile = caKeyPair.getPemCertificateFile();
    securityConfig.pemSigningCertificateFile = caKeyPair.getPemCertificateFile();
    securityConfig.pemSigningPrivateKeyFile = caKeyPair.getPemPrivateKeyFile();

    chord_tooling::AgentEntry agentEntry;
    agentEntry.pemCertificateFile = agentKeyPair.getPemCertificateFile();
    agentEntry.pemPrivateKeyFile = agentKeyPair.getPemPrivateKeyFile();
    agentEntry.agentLocation = chord_common::TransportLocation::forUnix(testerDirectory / "agent.sock");
    agentEntry.idleTimeout = absl::Seconds(15);

    chord_sandbox::IsolateOptions options;
    options.runDirectory = testerDirectory;
    options.agentPath = std::filesystem::path(CHORD_AGENT_EXECUTABLE);
    options.agentServerNameOverride = agentServerName;

    // initialize the sandbox
    auto spawnIsolateResult = chord_sandbox::ChordIsolate::spawn(
        agentServerName,
        std::make_shared<const chord_tooling::SecurityConfig>(std::move(securityConfig)),
        std::make_shared<chord_tooling::AgentEntry>(std::move(agentEntry)),
        options);
    ASSERT_THAT (spawnIsolateResult, tempo_test::IsResult());
    auto isolate = spawnIsolateResult.getResult();

    // shut down the sandbox
    ASSERT_THAT (isolate->shutdown(), tempo_test::IsOk());
}

TEST_F(ChordIsolate, SpawnRemoteMachine)
{
    auto result = chord_test::ChordSandboxTester::runSingleModuleInSandbox(R"(
        42
    )", options);

    ASSERT_THAT (result, tempo_test::ContainsResult(
        RunMachine(tempo_utils::StatusCode::kOk)));
}
