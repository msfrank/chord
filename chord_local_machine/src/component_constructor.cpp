
#include <chord_local_machine/component_constructor.h>
#include <tempo_security/ecc_private_key_generator.h>
#include <tempo_security/generate_utils.h>

std::shared_ptr<lyric_runtime::InterpreterState>
ComponentConstructor::createInterpreterState(
    const lyric_runtime::InterpreterStateOptions &interpreterOptions,
    const lyric_common::AssemblyLocation &mainLocation) const
{
    TU_ASSERT (mainLocation.isValid());
    auto createInterpreter = lyric_runtime::InterpreterState::create(interpreterOptions, mainLocation);
    TU_ASSERT (createInterpreter.isResult());
    return createInterpreter.getResult();
}

std::shared_ptr<LocalMachine>
ComponentConstructor::createLocalMachine(
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

std::unique_ptr<RemotingService>
ComponentConstructor::createRemotingService(
    bool startSuspended,
    std::shared_ptr<LocalMachine> localMachine,
    uv_async_t *initComplete) const
{
    TU_ASSERT (localMachine != nullptr);
    TU_ASSERT (initComplete != nullptr);
    return std::make_unique<RemotingService>(startSuspended, localMachine, initComplete);
}

std::shared_ptr<grpc::ChannelInterface>
ComponentConstructor:: createCustomChannel(
    std::string_view targetEndpoint,
    const std::shared_ptr<grpc::ChannelCredentials> &channelCreds,
    const grpc::ChannelArguments &channelArgs) const
{
    TU_ASSERT (!targetEndpoint.empty());
    TU_ASSERT (channelCreds != nullptr);
    return grpc::CreateCustomChannel(grpc::string(targetEndpoint), channelCreds, channelArgs);
}

std::unique_ptr<chord_invoke::InvokeService::StubInterface>
ComponentConstructor::createInvokeStub(std::shared_ptr<grpc::ChannelInterface> customChannel) const
{
    TU_ASSERT (customChannel != nullptr);
    return chord_invoke::InvokeService::NewStub(customChannel);
}

std::shared_ptr<GrpcBinder>
ComponentConstructor::createGrpcBinder(
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