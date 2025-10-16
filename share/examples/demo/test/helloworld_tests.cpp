#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_test/chord_sandbox_tester.h>
#include <chord_test/matchers.h>
#include <lyric_test/lyric_tester.h>
#include <lyric_test/matchers.h>
#include <tempo_test/tempo_test.h>
#include <tempo_config/workspace_config.h>

class Helloworld : public ::testing::Test {
protected:
    chord_test::SandboxTesterOptions options;

    void SetUp() override {
        options.isTemporary = false;
    }
};

TEST_F(Helloworld, SpawnHelloworld)
{
    std::filesystem::path packagePath(CHORD_DEMO_PACKAGE_PATH);
    auto result = chord_test::ChordSandboxTester::spawnSingleProgramInSandbox(packagePath, options);

    ASSERT_THAT (result, tempo_test::ContainsResult(
        SpawnMachine(tempo_utils::StatusCode::kOk)));
}
