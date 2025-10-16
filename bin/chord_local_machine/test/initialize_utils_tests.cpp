#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_local_machine/initialize_utils.h>
#include <chord_local_machine/run_utils.h>
#include <lyric_bootstrap/bootstrap_loader.h>
#include <lyric_common/module_location.h>
#include <lyric_runtime/chain_loader.h>
#include <tempo_test/status_matchers.h>
#include <tempo_utils/tempdir_maker.h>
#include <zuri_distributor/package_cache_loader.h>

#include "test_mocks.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SaveArgPointee;

class InitializeUtilsTests : public ::testing::Test {
protected:
    std::unique_ptr<tempo_utils::TempdirMaker> testDirectory;
    void SetUp() override {
        testDirectory = std::make_unique<tempo_utils::TempdirMaker>(
            std::filesystem::current_path(), "tester.XXXXXXXX");
    }
    void TearDown() override {
    }
};

TEST_F(InitializeUtilsTests, MakeInterpreterStateWithNoPackageDirectories)
{
    ChordLocalMachineConfig chordLocalMachineConfig;
    chordLocalMachineConfig.mainLocation = zuri_packager::PackageSpecifier::fromString(TEST1_ZPK).toUrl();
    chordLocalMachineConfig.runDirectory = testDirectory->getTempdir();

    MockComponentConstructor componentConstructor;
    std::shared_ptr<lyric_runtime::AbstractLoader> systemLoader;
    std::shared_ptr<lyric_runtime::AbstractLoader> applicationLoader;
    lyric_runtime::InterpreterStateOptions options;
    EXPECT_CALL (componentConstructor, createInterpreterState(_, _, _))
        .WillOnce(Invoke([&](auto sys_, auto app_, const auto &opts_) -> auto {
            systemLoader = sys_;
            applicationLoader = app_;
            options = opts_;
            return std::shared_ptr<lyric_runtime::InterpreterState>();
        }));

    std::shared_ptr<lyric_runtime::InterpreterState> interpreterState;
    ASSERT_THAT (make_interpreter_state(interpreterState, componentConstructor, chordLocalMachineConfig),
        tempo_test::IsOk());

    ASSERT_TRUE (systemLoader != nullptr);

    auto *chainLoader = dynamic_cast<lyric_runtime::ChainLoader *>(applicationLoader.get());
    ASSERT_TRUE (chainLoader != nullptr);
    ASSERT_EQ (1, chainLoader->numLoaders());
    auto appLoader0 = std::static_pointer_cast<zuri_distributor::PackageCacheLoader>(chainLoader->getLoader(0));
    ASSERT_EQ (testDirectory->getTempdir() / "cache", appLoader0->getPackageCache()->getCacheDirectory());

    ASSERT_EQ (chordLocalMachineConfig.mainLocation, options.mainLocation.toUrl());
}

TEST_F(InitializeUtilsTests, MakeInterpreterStateWithPackageDirectory)
{
    ChordLocalMachineConfig chordLocalMachineConfig;
    chordLocalMachineConfig.mainLocation = zuri_packager::PackageSpecifier::fromString(TEST1_ZPK).toUrl();
    chordLocalMachineConfig.runDirectory = testDirectory->getTempdir();
    auto packageCacheDirectory = testDirectory->getTempdir() / "cache2";
    ASSERT_TRUE (std::filesystem::create_directory(packageCacheDirectory));
    chordLocalMachineConfig.packageCacheDirectories = { packageCacheDirectory };

    MockComponentConstructor componentConstructor;
    std::shared_ptr<lyric_runtime::AbstractLoader> systemLoader;
    std::shared_ptr<lyric_runtime::AbstractLoader> applicationLoader;
    lyric_runtime::InterpreterStateOptions options;
    EXPECT_CALL (componentConstructor, createInterpreterState(_, _, _))
        .WillOnce(Invoke([&](const auto& sys_, const auto &app_, const auto &opts_) -> auto {
            systemLoader = sys_;
            applicationLoader = app_;
            options = opts_;
            return std::shared_ptr<lyric_runtime::InterpreterState>();
        }));

    std::shared_ptr<lyric_runtime::InterpreterState> interpreterState;
    ASSERT_THAT (make_interpreter_state(interpreterState, componentConstructor, chordLocalMachineConfig),
        tempo_test::IsOk());

    ASSERT_TRUE (systemLoader != nullptr);

    auto *chainLoader = dynamic_cast<lyric_runtime::ChainLoader *>(applicationLoader.get());
    ASSERT_TRUE (chainLoader != nullptr);
    ASSERT_EQ (2, chainLoader->numLoaders());
    auto appLoader0 = std::static_pointer_cast<zuri_distributor::PackageCacheLoader>(chainLoader->getLoader(0));
    ASSERT_EQ (testDirectory->getTempdir() / "cache", appLoader0->getPackageCache()->getCacheDirectory());
    auto appLoader1 = std::static_pointer_cast<zuri_distributor::PackageCacheLoader>(chainLoader->getLoader(1));
    ASSERT_EQ (packageCacheDirectory, appLoader1->getPackageCache()->getCacheDirectory());

    ASSERT_EQ (chordLocalMachineConfig.mainLocation, options.mainLocation.toUrl());
}

TEST_F(InitializeUtilsTests, MakeLocalMachine)
{
    ChordLocalMachineConfig chordLocalMachineConfig;
    chordLocalMachineConfig.mainLocation = zuri_packager::PackageSpecifier::fromString(TEST1_ZPK).toUrl();
    chordLocalMachineConfig.runDirectory = testDirectory->getTempdir();
    chordLocalMachineConfig.machineUrl = tempo_utils::Url::fromString("dev.zuri.machine:xxx");
    chordLocalMachineConfig.startSuspended = true;

    ChordLocalMachineData chordLocalMachineData;
    chordLocalMachineData.interpreterState = std::make_shared<MockInterpreterState>();

    AsyncQueue<RunnerReply> processor;

    MockComponentConstructor componentConstructor;
    tempo_utils::Url machineUrl;
    bool startSuspended;
    std::shared_ptr<lyric_runtime::InterpreterState> interpreterState;
    AbstractMessageSender<RunnerReply> *processorPtr;
    EXPECT_CALL (componentConstructor, createLocalMachine(_, _, _, _))
        .WillOnce([&](const auto &machineUrl_, bool startSuspended_, auto &interpreterState_, auto *processorPtr_) -> auto {
            machineUrl = machineUrl_;
            startSuspended = startSuspended_;
            interpreterState = interpreterState_;
            processorPtr = processorPtr_;
            return std::shared_ptr<LocalMachine>();
        });

    std::shared_ptr<LocalMachine> localMachine;
    ASSERT_THAT (make_local_machine(localMachine, componentConstructor, chordLocalMachineConfig,
        chordLocalMachineData.interpreterState, &processor),
        tempo_test::IsOk());

    ASSERT_EQ (chordLocalMachineConfig.machineUrl, machineUrl);
    ASSERT_EQ (chordLocalMachineConfig.startSuspended, startSuspended);
    ASSERT_EQ (chordLocalMachineData.interpreterState, interpreterState);
    ASSERT_EQ (&processor, processorPtr);
}

TEST_F(InitializeUtilsTests, MakeInvokeServiceStub)
{
    ChordLocalMachineConfig chordLocalMachineConfig;
    chordLocalMachineConfig.runDirectory = testDirectory->getTempdir();
    chordLocalMachineConfig.machineUrl = tempo_utils::Url::fromString("dev.zuri.machine:xxx");

    ChordLocalMachineData chordLocalMachineData;
    chordLocalMachineData.customChannel = std::make_shared<MockChannel>();

    MockComponentConstructor componentConstructor;
    std::shared_ptr<grpc::ChannelInterface> customChannel;
    EXPECT_CALL (componentConstructor, createInvokeStub(_))
        .WillOnce([&](auto customChannel_) -> auto {
            customChannel = customChannel_;
            return std::unique_ptr<chord_invoke::InvokeService::StubInterface>();
        });

    std::unique_ptr<chord_invoke::InvokeService::StubInterface> stub;
    ASSERT_THAT (make_invoke_service_stub(stub, componentConstructor, chordLocalMachineConfig,
        chordLocalMachineData.customChannel),
        tempo_test::IsOk());

    ASSERT_EQ (chordLocalMachineData.customChannel, customChannel);
}

TEST_F(InitializeUtilsTests, MakeGrpcBinder)
{
    ChordLocalMachineConfig chordLocalMachineConfig;
    chordLocalMachineConfig.runDirectory = testDirectory->getTempdir();
    chordLocalMachineConfig.machineUrl = tempo_utils::Url::fromString("dev.zuri.machine:xxx");

    ChordLocalMachineData chordLocalMachineData;
    chordLocalMachineData.csrKeyPair = tempo_security::CSRKeyPair(
        tempo_security::KeyType::ECC, "/key", "/req");
    chordLocalMachineData.invokeStub = std::make_unique<MockInvokeStub>();
    chordLocalMachineData.remotingService = std::make_unique<MockRemotingService>();

    std::string binderEndpoint;
    lyric_common::RuntimePolicy runtimePolicy;
    std::filesystem::path pemPrivateKeyFile;
    std::filesystem::path pemRootCABundleFile;
    chord_remoting::RemotingService::CallbackService *remotingService;

    MockComponentConstructor componentConstructor;
    EXPECT_CALL (componentConstructor, createGrpcBinder(_, _, _, _, _))
        .WillOnce([&](auto binderEndpoint_,
                      const auto &runtimePolicy_,
                      const auto &pemPrivateKeyFile_,
                      const auto &pemRootCABundleFile_,
                      auto *remotingService_) -> auto {
            binderEndpoint = binderEndpoint_;
            pemPrivateKeyFile = pemPrivateKeyFile_;
            pemRootCABundleFile = pemRootCABundleFile_;
            remotingService = remotingService_;
            return std::shared_ptr<GrpcBinder>();
        });

    std::shared_ptr<GrpcBinder> grpcBinder;
    ASSERT_THAT (make_grpc_binder(
        grpcBinder, componentConstructor, chordLocalMachineConfig, chordLocalMachineData.csrKeyPair,
        chordLocalMachineData.invokeStub.get(), chordLocalMachineData.remotingService.get()),
        tempo_test::IsOk());
}
