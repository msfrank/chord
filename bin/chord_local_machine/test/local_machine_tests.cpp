#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_local_machine/local_machine.h>
#include <lyric_bootstrap/bootstrap_loader.h>
#include <lyric_runtime/static_loader.h>
#include <tempo_test/status_matchers.h>
#include <tempo_utils/tempdir_maker.h>
#include <zuri_distributor/package_cache_loader.h>

class LocalMachineTests : public ::testing::Test {
protected:
    std::unique_ptr<tempo_utils::TempdirMaker> testDirectory;
    void SetUp() override {
        testDirectory = std::make_unique<tempo_utils::TempdirMaker>(
            std::filesystem::current_path(), "tester.XXXXXXXX");
    }
    void TearDown() override {
    }
};

TEST_F(LocalMachineTests, Construct)
{
    uv_loop_t loop;
    ASSERT_EQ (0, uv_loop_init(&loop));

    std::shared_ptr<zuri_distributor::PackageCache> packageCache;
    TU_ASSIGN_OR_RAISE (packageCache, zuri_distributor::PackageCache::openOrCreate(
        testDirectory->getTempdir(), "pkgcache"));

    std::shared_ptr<zuri_packager::PackageReader> reader;
    TU_ASSIGN_OR_RAISE (reader, zuri_packager::PackageReader::open(TEST1_ZPK));

    TU_RAISE_IF_STATUS (packageCache->installPackage(reader));

    zuri_packager::PackageSpecifier specifier;
    TU_ASSIGN_OR_RAISE (specifier, reader->readPackageSpecifier());
    lyric_common::ModuleLocation programMain;
    TU_ASSIGN_OR_RAISE (programMain, reader->readProgramMain());
    auto mainLocation = lyric_common::ModuleLocation::fromUrl(
        specifier.toUrl()
            .resolve(programMain.getPath()));

    lyric_runtime::InterpreterStateOptions options;
    options.mainLocation = mainLocation;

    auto systemLoader = std::make_shared<lyric_bootstrap::BootstrapLoader>();
    auto applicationLoader = std::make_shared<zuri_distributor::PackageCacheLoader>(packageCache);

    std::shared_ptr<lyric_runtime::InterpreterState> state;
    TU_ASSIGN_OR_RAISE(state, lyric_runtime::InterpreterState::create(
        systemLoader, applicationLoader, options));

    chord_machine::AsyncQueue<chord_machine::RunnerReply> processor;
    ASSERT_THAT (processor.initialize(&loop), tempo_test::IsOk());

    auto machineUrl = tempo_utils::Url::fromString("foo");
    auto machine = std::make_shared<chord_machine::LocalMachine>(machineUrl, true, state, &processor);
    ASSERT_EQ (chord_machine::InterpreterRunnerState::INITIAL, machine->getRunnerState());
    ASSERT_EQ (machineUrl, machine->getMachineUrl());

    // invoke destructor to terminate the runner thread
    machine.reset();

    auto *message = processor.waitForMessage();
    ASSERT_EQ (chord_machine::RunnerReply::MessageType::Cancelled, message->type);
    delete message;
}

TEST_F(LocalMachineTests, Start)
{
    uv_loop_t loop;
    ASSERT_EQ (0, uv_loop_init(&loop));

    std::shared_ptr<zuri_distributor::PackageCache> packageCache;
    TU_ASSIGN_OR_RAISE (packageCache, zuri_distributor::PackageCache::openOrCreate(
        testDirectory->getTempdir(), "pkgcache"));

    std::shared_ptr<zuri_packager::PackageReader> reader;
    TU_ASSIGN_OR_RAISE (reader, zuri_packager::PackageReader::open(TEST1_ZPK));
    TU_RAISE_IF_STATUS (packageCache->installPackage(reader));

    zuri_packager::PackageSpecifier specifier;
    TU_ASSIGN_OR_RAISE (specifier, reader->readPackageSpecifier());
    lyric_common::ModuleLocation programMain;
    TU_ASSIGN_OR_RAISE (programMain, reader->readProgramMain());
    auto mainLocation = lyric_common::ModuleLocation::fromUrl(
        specifier.toUrl()
            .resolve(programMain.getPath()));

    lyric_runtime::InterpreterStateOptions options;
    options.mainLocation = mainLocation;

    auto systemLoader = std::make_shared<lyric_bootstrap::BootstrapLoader>();
    auto applicationLoader = std::make_shared<zuri_distributor::PackageCacheLoader>(packageCache);

    std::shared_ptr<lyric_runtime::InterpreterState> state;
    TU_ASSIGN_OR_RAISE(state, lyric_runtime::InterpreterState::create(
        systemLoader, applicationLoader, options));

    chord_machine::AsyncQueue<chord_machine::RunnerReply> processor;
    ASSERT_THAT (processor.initialize(&loop), tempo_test::IsOk());

    auto machineUrl = tempo_utils::Url::fromString("foo");
    auto machine = std::make_shared<chord_machine::LocalMachine>(machineUrl, true, state, &processor);

    // start the machine and wait for state change
    ASSERT_THAT (machine->resume(), tempo_test::IsOk());

    auto *message1 = processor.waitForMessage();
    ASSERT_EQ (chord_machine::RunnerReply::MessageType::Running, message1->type);
    delete message1;

    // // invoke destructor to terminate the runner thread
    // machine.reset();

    auto *message2 = processor.waitForMessage();
    ASSERT_EQ (chord_machine::RunnerReply::MessageType::Completed, message2->type);
    delete message2;
}