#include <gtest/gtest.h>

#include <chord_local_machine/local_machine.h>
#include <lyric_bootstrap/bootstrap_loader.h>
#include <lyric_runtime/static_loader.h>
#include <zuri_distributor/package_cache_loader.h>

TEST(LocalMachine, Construct)
{
    tempo_utils::Status status;

    auto machineUrl = tempo_utils::Url::fromString("foo");

    uv_loop_t loop;
    uv_loop_init(&loop);

    lyric_runtime::InterpreterStateOptions options;
    options.mainLocation = lyric_common::ModuleLocation::fromString(CHORD_DEMO_PACKAGE_PATH);

    auto systemLoader = std::make_shared<lyric_bootstrap::BootstrapLoader>();
    auto applicationLoader = std::make_shared<lyric_runtime::StaticLoader>();

    std::shared_ptr<lyric_runtime::InterpreterState> state;
    TU_ASSIGN_OR_RAISE(state, lyric_runtime::InterpreterState::create(
        systemLoader, applicationLoader, options));

    AsyncQueue<RunnerReply> processor;
    ASSERT_TRUE (processor.initialize(&loop).isOk());

    auto machine = std::make_shared<LocalMachine>(machineUrl, true, state, &processor);
    ASSERT_EQ (InterpreterRunnerState::INITIAL, machine->getRunnerState());
    ASSERT_EQ (machineUrl, machine->getMachineUrl());

    // invoke destructor to terminate the runner thread
    machine.reset();

    auto *message = processor.waitForMessage();
    ASSERT_EQ (RunnerReply::MessageType::Cancelled, message->type);
    delete message;
}

TEST(LocalMachine, Start)
{
    tempo_utils::Status status;

    auto machineUrl = tempo_utils::Url::fromString("foo");

    uv_loop_t loop;
    uv_loop_init(&loop);

    lyric_runtime::InterpreterStateOptions options;
    options.mainLocation = lyric_common::ModuleLocation::fromString(CHORD_DEMO_PACKAGE_PATH);

    auto systemLoader = std::make_shared<lyric_bootstrap::BootstrapLoader>();
    auto applicationLoader = std::make_shared<lyric_runtime::StaticLoader>();

    std::shared_ptr<lyric_runtime::InterpreterState> state;
    TU_ASSIGN_OR_RAISE(state, lyric_runtime::InterpreterState::create(
        systemLoader, applicationLoader, options));

    AsyncQueue<RunnerReply> processor;
    ASSERT_TRUE (processor.initialize(&loop).isOk());

    auto machine = std::make_shared<LocalMachine>(machineUrl, true, state, &processor);

    // start the machine and wait for state change
    ASSERT_TRUE (machine->resume().isOk());
    auto *message1 = processor.waitForMessage();
    ASSERT_EQ (RunnerReply::MessageType::Running, message1->type);
    delete message1;

    // invoke destructor to terminate the runner thread
    machine.reset();

    auto *message2 = processor.waitForMessage();
    ASSERT_EQ (RunnerReply::MessageType::Completed, message2->type);
    delete message2;
}