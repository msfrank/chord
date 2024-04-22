#include <gtest/gtest.h>

#include <chord_local_machine/local_machine.h>
#include <lyric_bootstrap/bootstrap_loader.h>
#include <lyric_packaging/package_loader.h>
#include <lyric_runtime/chain_loader.h>

TEST(LocalMachine, Construct)
{
    tempo_utils::Status status;

    auto machineUrl = tempo_utils::Url::fromString("foo");

    uv_loop_t loop;
    uv_loop_init(&loop);

    lyric_runtime::InterpreterStateOptions options;

    std::vector<std::shared_ptr<lyric_runtime::AbstractLoader>> loaderChain;
    loaderChain.push_back(std::make_shared<lyric_bootstrap::BootstrapLoader>());
    loaderChain.push_back(std::make_shared<lyric_packaging::PackageLoader>());
    options.loader = std::make_shared<lyric_runtime::ChainLoader>(loaderChain);

    auto location = lyric_common::AssemblyLocation::fromString(EXAMPLES_DEMO_PACKAGE);
    std::shared_ptr<lyric_runtime::InterpreterState> state;
    TU_ASSIGN_OR_RAISE(state, lyric_runtime::InterpreterState::create(options, location));

    AsyncQueue<RunnerReply> processor;
    ASSERT_TRUE (processor.initialize(&loop).isOk());

    auto machine = std::make_shared<LocalMachine>(machineUrl, state, &processor);
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

    std::vector<std::shared_ptr<lyric_runtime::AbstractLoader>> loaderChain;
    loaderChain.push_back(std::make_shared<lyric_bootstrap::BootstrapLoader>());
    loaderChain.push_back(std::make_shared<lyric_packaging::PackageLoader>());
    options.loader = std::make_shared<lyric_runtime::ChainLoader>(loaderChain);

    auto location = lyric_common::AssemblyLocation::fromString(EXAMPLES_DEMO_PACKAGE);
    std::shared_ptr<lyric_runtime::InterpreterState> state;
    TU_ASSIGN_OR_RAISE(state, lyric_runtime::InterpreterState::create(options, location));

    AsyncQueue<RunnerReply> processor;
    ASSERT_TRUE (processor.initialize(&loop).isOk());

    auto machine = std::make_shared<LocalMachine>(machineUrl, state, &processor);

    // start the machine and wait for state change
    ASSERT_TRUE (machine->start().isOk());
    auto *message1 = processor.waitForMessage();
    ASSERT_EQ (RunnerReply::MessageType::Running, message1->type);
    delete message1;

    // invoke destructor to terminate the runner thread
    machine.reset();

    auto *message2 = processor.waitForMessage();
    ASSERT_EQ (RunnerReply::MessageType::Completed, message2->type);
    delete message2;
}