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
    lyric_common::AssemblyLocation location;

    void SetUp() override {
        location = lyric_common::AssemblyLocation::fromString(HELLOWORLD_PACKAGE_URL);
        tempo_config::WorkspaceConfigOptions workspaceOptions;
        workspaceOptions.distConfigDirectoryPath = ZURI_INSTALL_CONFIG_DIR;
        workspaceOptions.distVendorConfigDirectoryPath = ZURI_INSTALL_VENDOR_CONFIG_DIR;
        workspaceOptions.toolLocator = {"zuri-build"};
        std::shared_ptr<tempo_config::WorkspaceConfig> config;
        TU_ASSIGN_OR_RAISE (config, tempo_config::WorkspaceConfig::load(WORKSPACE_CONFIG_FILE, workspaceOptions));
        options.isTemporary = false;
        options.buildConfig = config->getToolConfig();
        options.buildVendorConfig = config->getVendorConfig();
    }
};

TEST_F(Helloworld, SpawnHelloworld)
{
    auto result = chord_test::ChordSandboxTester::spawnSingleModuleInSandbox(location, options);

    ASSERT_THAT (result, tempo_test::ContainsResult(SpawnMachine(tempo_utils::StatusCode::kOk)));
}
