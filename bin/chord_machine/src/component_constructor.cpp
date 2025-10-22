
#include <chord_machine/component_constructor.h>
#include <tempo_security/ecc_private_key_generator.h>
#include <tempo_security/generate_utils.h>

std::shared_ptr<lyric_runtime::InterpreterState>
chord_machine::ComponentConstructor::createInterpreterState(
    std::shared_ptr<lyric_runtime::AbstractLoader> systemLoader,
    std::shared_ptr<lyric_runtime::AbstractLoader> applicationLoader,
    const lyric_runtime::InterpreterStateOptions &interpreterOptions) const
{
    TU_ASSERT (systemLoader != nullptr);
    TU_ASSERT (applicationLoader != nullptr);
    TU_ASSERT (interpreterOptions.mainLocation.isValid());
    auto createInterpreter = lyric_runtime::InterpreterState::create(
        std::move(systemLoader), std::move(applicationLoader), interpreterOptions);
    TU_ASSERT (createInterpreter.isResult());
    return createInterpreter.getResult();
}

std::shared_ptr<chord_machine::LocalMachine>
chord_machine::ComponentConstructor::createLocalMachine(
    const tempo_utils::Url &machineUrl,
    bool startSuspended,
    std::shared_ptr<lyric_runtime::InterpreterState> &interpreterState,
    AbstractMessageSender<RunnerReply> *processor) const
{
    TU_ASSERT (machineUrl.isValid());
    TU_ASSERT (interpreterState != nullptr);
    TU_ASSERT (processor != nullptr);
    return std::make_shared<LocalMachine>(machineUrl, startSuspended, interpreterState, processor);
}

std::unique_ptr<chord_machine::RemotingService>
chord_machine::ComponentConstructor::createRemotingService(
    bool startSuspended,
    std::shared_ptr<LocalMachine> localMachine,
    uv_async_t *initComplete) const
{
    TU_ASSERT (localMachine != nullptr);
    TU_ASSERT (initComplete != nullptr);
    return std::make_unique<RemotingService>(startSuspended, std::move(localMachine), initComplete);
}

std::shared_ptr<grpc::ChannelInterface>
chord_machine::ComponentConstructor:: createCustomChannel(
    std::string_view targetEndpoint,
    const std::shared_ptr<grpc::ChannelCredentials> &channelCreds,
    const grpc::ChannelArguments &channelArgs) const
{
    TU_ASSERT (!targetEndpoint.empty());
    TU_ASSERT (channelCreds != nullptr);
    return grpc::CreateCustomChannel(grpc::string(targetEndpoint), channelCreds, channelArgs);
}

std::unique_ptr<chord_invoke::InvokeService::StubInterface>
chord_machine::ComponentConstructor::createInvokeStub(std::shared_ptr<grpc::ChannelInterface> customChannel) const
{
    TU_ASSERT (customChannel != nullptr);
    return chord_invoke::InvokeService::NewStub(customChannel);
}

std::shared_ptr<chord_machine::GrpcBinder>
chord_machine::ComponentConstructor::createGrpcBinder(
    std::string_view binderEndpoint,
    const lyric_common::RuntimePolicy &runtimePolicy,
    const std::filesystem::path &pemPrivateKeyFile,
    const std::filesystem::path &pemRootCABundleFile,
    chord_remoting::RemotingService::CallbackService *remotingService) const
{
    TU_ASSERT (!binderEndpoint.empty());
    TU_ASSERT (!pemPrivateKeyFile.empty());
    TU_ASSERT (!pemRootCABundleFile.empty());
    TU_ASSERT (remotingService != nullptr);

    return std::make_shared<GrpcBinder>(
        binderEndpoint, runtimePolicy, pemPrivateKeyFile, pemRootCABundleFile, remotingService);
}