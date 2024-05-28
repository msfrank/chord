#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <lyric_test/lyric_tester.h>
#include <lyric_test/matchers.h>
#include <tempo_test/tempo_test.h>
#include <tempo_config/workspace_config.h>

class NetHttpManager : public ::testing::Test {
protected:
    lyric_test::TesterOptions testerOptions;

    void SetUp() override {
        auto loadWorkspaceResult = tempo_config::WorkspaceConfig::load(
            TESTER_CONFIG_PATH, {});
        ASSERT_TRUE (loadWorkspaceResult.isResult());
        auto config = loadWorkspaceResult.getResult();

        testerOptions.buildConfig = config->getToolConfig();
        testerOptions.buildVendorConfig = config->getVendorConfig();
    }
};

TEST_F(NetHttpManager, EvaluateGet)
{
    auto result = lyric_test::LyricTester::runSingleModule(R"(
        import from "//std/system" ...
        import from "//net/http" ...
        val manager: Manager = Manager{}
        val fut: Future[Response] = manager.Get(`http://neverssl.com/`)
        match Await(fut) {
            case resp: Response     resp.StatusCode
            else                    nil
        }
    )", testerOptions);

    ASSERT_THAT (result, tempo_test::ContainsResult(RunModule(DataCellInt(200))));
}
