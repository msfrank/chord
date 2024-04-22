#ifndef CHORD_LOCAL_MACHINE_COMPONENT_CONSTRUCTOR_H
#define CHORD_LOCAL_MACHINE_COMPONENT_CONSTRUCTOR_H

#include <chord_invoke/invoke_service.grpc.pb.h>
#include <tempo_security/csr_key_pair.h>
#include <tempo_utils/status.h>
#include <tempo_utils/url.h>

#include "chord_local_machine.h"
#include "config_utils.h"
#include "grpc_binder.h"
#include "local_machine.h"

class ComponentConstructor {
public:
    virtual ~ComponentConstructor() = default;

    virtual std::shared_ptr<lyric_runtime::InterpreterState> createInterpreterState(
        const lyric_runtime::InterpreterStateOptions &interpreterOptions,
        const lyric_common::AssemblyLocation &mainLocation) const;

    virtual std::shared_ptr<LocalMachine> createLocalMachine(
        const tempo_utils::Url &machineUrl,
        std::shared_ptr<lyric_runtime::InterpreterState> &interpreterState,
        AbstractMessageSender<RunnerReply> *processor) const;

    virtual std::shared_ptr<grpc::ChannelInterface> createCustomChannel(
        std::string_view targetEndpoint,
        const std::shared_ptr<grpc::ChannelCredentials> &channelCreds,
        const grpc::ChannelArguments &channelArgs) const;

    virtual std::unique_ptr<chord_invoke::InvokeService::StubInterface> createInvokeStub(
        std::shared_ptr<grpc::ChannelInterface> customChannel) const;

    virtual std::unique_ptr<RemotingService> createRemotingService(
        uv_async_t *initComplete) const;

    virtual std::shared_ptr<GrpcBinder> createGrpcBinder(
        std::string_view binderEndpoint,
        const lyric_common::RuntimePolicy &runtimePolicy,
        const std::filesystem::path &pemPrivateKeyFile,
        const std::filesystem::path &pemRootCABundleFile,
        chord_remoting::RemotingService::CallbackService *remotingService) const;
};

#endif // CHORD_LOCAL_MACHINE_COMPONENT_CONSTRUCTOR_H
