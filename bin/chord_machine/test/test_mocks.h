#ifndef CHORD_MACHINE_TEST_MOCKS_H
#define CHORD_MACHINE_TEST_MOCKS_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_machine/initialize_utils.h>
#include <chord_machine/run_utils.h>
#include <lyric_runtime/chain_loader.h>
#include <lyric_runtime/abstract_loader.h>
#include <lyric_bootstrap/bootstrap_loader.h>
#include <tempo_test/status_matchers.h>

class MockInterpreterState : public lyric_runtime::InterpreterState {
public:
    MOCK_METHOD (
        lyric_runtime::StackfulCoroutine *,
    currentCoro,
    (),
    (const, override));
};

class MockChannel : public grpc::ChannelInterface {
public:
    MOCK_METHOD (
        grpc_connectivity_state,
        GetState, (
    bool try_to_connect),
    (override));

    MOCK_METHOD (
        grpc::internal::Call,
        CreateCall, (
    const grpc::internal::RpcMethod& method,
        grpc::ClientContext* context,
    grpc::CompletionQueue* cq),
    (override));

    MOCK_METHOD (
        void,
        PerformOpsOnCall, (
        grpc::internal::CallOpSetInterface* ops,
        grpc::internal::Call* call),
    (override));

    MOCK_METHOD (
        void*,
        RegisterMethod, (
        const char* method),
    (override));

    MOCK_METHOD (
        void,
        NotifyOnStateChangeImpl, (
        grpc_connectivity_state last_observed,
        gpr_timespec deadline,
        grpc::CompletionQueue* cq,
        void* tag),
    (override));

    MOCK_METHOD (
        bool,
        WaitForStateChangeImpl, (
        grpc_connectivity_state last_observed,
        gpr_timespec deadline),
    (override));

};

class MockInvokeStub : public chord_invoke::InvokeService::StubInterface {
public:
    MOCK_METHOD (
        grpc::Status,
        IdentifyAgent, (
            grpc::ClientContext* context,
            const chord_invoke::IdentifyAgentRequest& request,
            chord_invoke::IdentifyAgentResult* response),
        (override));

    MOCK_METHOD (
        grpc::Status,
        CreateMachine, (
            grpc::ClientContext* context,
            const chord_invoke::CreateMachineRequest& request,
            chord_invoke::CreateMachineResult* response),
        (override));

    MOCK_METHOD (
        grpc::Status,
        SignCertificates, (
            grpc::ClientContext* context,
            const chord_invoke::SignCertificatesRequest& request,
            chord_invoke::SignCertificatesResult* response),
        (override));

    MOCK_METHOD (
        grpc::Status,
        AdvertiseEndpoints, (
            grpc::ClientContext* context,
            const chord_invoke::AdvertiseEndpointsRequest& request,
            chord_invoke::AdvertiseEndpointsResult* response),
        (override));

    MOCK_METHOD (
        grpc::Status,
        RunMachine, (
            grpc::ClientContext* context,
            const chord_invoke::RunMachineRequest& request,
            chord_invoke::RunMachineResult* response),
        (override));

    MOCK_METHOD (
        grpc::Status,
        DeleteMachine, (
            grpc::ClientContext* context,
            const chord_invoke::DeleteMachineRequest& request,
            chord_invoke::DeleteMachineResult* response),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface<chord_invoke::IdentifyAgentResult>*,
        AsyncIdentifyAgentRaw, (
            grpc::ClientContext* context,
            const chord_invoke::IdentifyAgentRequest& request,
            grpc::CompletionQueue* cq),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface<chord_invoke::IdentifyAgentResult>*,
        PrepareAsyncIdentifyAgentRaw, (
            grpc::ClientContext* context,
            const chord_invoke::IdentifyAgentRequest& request,
            grpc::CompletionQueue* cq),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface< chord_invoke::CreateMachineResult>*,
        AsyncCreateMachineRaw, (
            grpc::ClientContext* context,
            const chord_invoke::CreateMachineRequest& request,
            grpc::CompletionQueue* cq),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface<chord_invoke::CreateMachineResult>*,
        PrepareAsyncCreateMachineRaw, (
            grpc::ClientContext* context,
            const chord_invoke::CreateMachineRequest& request,
            grpc::CompletionQueue* cq),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface<chord_invoke::SignCertificatesResult>*,
        AsyncSignCertificatesRaw, (
            grpc::ClientContext* context,
            const chord_invoke::SignCertificatesRequest& request,
            grpc::CompletionQueue* cq),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface< chord_invoke::SignCertificatesResult>*,
        PrepareAsyncSignCertificatesRaw, (
            grpc::ClientContext* context,
            const chord_invoke::SignCertificatesRequest& request,
            grpc::CompletionQueue* cq),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface< chord_invoke::AdvertiseEndpointsResult>*,
        AsyncAdvertiseEndpointsRaw, (
            grpc::ClientContext* context,
            const chord_invoke::AdvertiseEndpointsRequest& request,
            grpc::CompletionQueue* cq),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface< chord_invoke::AdvertiseEndpointsResult>*,
        PrepareAsyncAdvertiseEndpointsRaw, (
            grpc::ClientContext* context,
            const chord_invoke::AdvertiseEndpointsRequest& request,
            grpc::CompletionQueue* cq),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface< chord_invoke::RunMachineResult>*,
        AsyncRunMachineRaw, (
            grpc::ClientContext* context,
            const chord_invoke::RunMachineRequest& request,
            grpc::CompletionQueue* cq),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface< chord_invoke::RunMachineResult>*,
        PrepareAsyncRunMachineRaw, (
            grpc::ClientContext* context,
            const chord_invoke::RunMachineRequest& request,
            grpc::CompletionQueue* cq),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface< chord_invoke::DeleteMachineResult>*,
        AsyncDeleteMachineRaw, (
            grpc::ClientContext* context,
            const chord_invoke::DeleteMachineRequest& request,
            grpc::CompletionQueue* cq),
        (override));

    MOCK_METHOD (
        grpc::ClientAsyncResponseReaderInterface< chord_invoke::DeleteMachineResult>*,
        PrepareAsyncDeleteMachineRaw, (
            grpc::ClientContext* context,
            const chord_invoke::DeleteMachineRequest& request,
            grpc::CompletionQueue* cq),
        (override));

};

class MockRemotingService : public chord_machine::RemotingService {
public:
    MOCK_METHOD (
        tempo_utils::Status,
        registerProtocolHandler, (
            const tempo_utils::Url &protocolUrl,
            std::shared_ptr<chord_common::AbstractProtocolHandler> handler,
            bool requiredAtLaunch),
        (override));

    MOCK_METHOD (
        bool,
        hasProtocolHandler, (
            const tempo_utils::Url &protocolUrl),
        (override));

    MOCK_METHOD (
        std::shared_ptr<chord_common::AbstractProtocolHandler>,
        getProtocolHandler, (
            const tempo_utils::Url &protocolUrl),
        (override));
};

class MockComponentConstructor : public chord_machine::ComponentConstructor {
public:
    MOCK_METHOD (
        std::shared_ptr<lyric_runtime::InterpreterState>,
        createInterpreterState, (
            std::shared_ptr<lyric_runtime::AbstractLoader> systemLoader,
            std::shared_ptr<lyric_runtime::AbstractLoader> applicationLoader,
            const lyric_runtime::InterpreterStateOptions &options),
        (const, override));

    MOCK_METHOD (
        std::shared_ptr<chord_machine::LocalMachine>,
        createLocalMachine, (
            const tempo_utils::Url &machineUrl,
            bool startSuspended,
            std::shared_ptr<lyric_runtime::InterpreterState> &interpreterState,
            chord_machine::AbstractMessageSender<chord_machine::RunnerReply> *processor),
        (const, override));

    MOCK_METHOD (
        std::shared_ptr<grpc::ChannelInterface>,
        createCustomChannel, (
            std::string_view targetEndpoint,
            const std::shared_ptr<grpc::ChannelCredentials> &channelCreds,
            const grpc::ChannelArguments &channelArgs),
        (const, override));

    MOCK_METHOD (
        std::unique_ptr<chord_invoke::InvokeService::StubInterface>,
        createInvokeStub, (
            std::shared_ptr<grpc::ChannelInterface> customChannel),
        (const, override));

    MOCK_METHOD (
        std::unique_ptr<chord_machine::RemotingService>,
        createRemotingService, (
            bool startSuspended,
            std::shared_ptr<chord_machine::LocalMachine> localMachine,
            uv_async_t *initComplete),
        (const, override));

    MOCK_METHOD (
        std::shared_ptr<chord_machine::GrpcBinder>,
        createGrpcBinder, (
            std::string_view binderEndpoint,
            const lyric_common::RuntimePolicy &runtimePolicy,
            const std::filesystem::path &pemPrivateKeyFile,
            const std::filesystem::path &pemRootCABundleFile,
            chord_remoting::RemotingService::CallbackService *remotingService),
        (const, override));
};

#endif // CHORD_MACHINE_TEST_MOCKS_H
