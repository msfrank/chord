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

#include "chord_sandbox/local_certificate_signer.h"

class ChordIsolate : public ::testing::Test {
protected:
    chord_test::SandboxTesterOptions options;
    std::unique_ptr<tempo_utils::TempdirMaker> tempdir;

    void SetUp() override {
        tempo_utils::init_logging(tempo_utils::LoggingConfiguration{});
        options.isTemporary = false;
        tempdir = std::make_unique<tempo_utils::TempdirMaker>("tester.XXXXXXXX");
        TU_ASSERT (tempdir->isValid());
    }
};

TEST_F(ChordIsolate, InitializeAndShutdown)
{
    auto testerDirectory = tempdir->getTempdir();
    auto agentPath = std::filesystem::path(CHORD_AGENT_EXECUTABLE);
    auto caCommonName = absl::StrCat(
        "ca.", tempo_utils::generate_name("XXXXXXXX"), ".test");

    tempo_security::ECCPrivateKeyGenerator keygen(tempo_security::ECCurveId::Prime256v1);

    tempo_security::CertificateKeyPair caKeyPair;
    TU_ASSIGN_OR_RAISE (caKeyPair, tempo_security::generate_self_signed_ca_key_pair(keygen,
        "ChordIsolate", "InitializeAndShutdown",
        caCommonName, 1, std::chrono::seconds{60 * 60 * 24}, -1,
        testerDirectory, "ca"));
    auto certificateSigner = std::make_shared<chord_sandbox::LocalCertificateSigner>(caKeyPair);
    auto pemRootCABundleFile = caKeyPair.getPemCertificateFile();

    auto idleTimeout = absl::Seconds(60);
    auto registrationTimeout = absl::Seconds(60);

    // initialize the sandbox
    auto spawnIsolateResult = chord_sandbox::ChordIsolate::spawn(
        "test", testerDirectory, agentPath, pemRootCABundleFile, certificateSigner,
        idleTimeout, registrationTimeout);;
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
