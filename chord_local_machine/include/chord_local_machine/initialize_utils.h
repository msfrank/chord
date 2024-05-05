#ifndef CHORD_LOCAL_MACHINE_INITIALIZE_UTILS_H
#define CHORD_LOCAL_MACHINE_INITIALIZE_UTILS_H

#include <filesystem>

#include <absl/container/flat_hash_set.h>
#include <uv.h>

#include <chord_invoke/invoke_service.grpc.pb.h>
#include <tempo_security/csr_key_pair.h>
#include <tempo_utils/status.h>
#include <tempo_utils/url.h>

#include "chord_local_machine.h"
#include "component_constructor.h"
#include "config_utils.h"
#include "grpc_binder.h"
#include "local_machine.h"

tempo_utils::Status make_interpreter_state(
    std::shared_ptr<lyric_runtime::InterpreterState> &interpreterState,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig);

tempo_utils::Status make_local_machine(
    std::shared_ptr<LocalMachine> &localMachine,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    std::shared_ptr<lyric_runtime::InterpreterState> interpreterState,
    AbstractMessageSender<RunnerReply> *processor);

tempo_utils::Status make_remoting_service(
    std::unique_ptr<RemotingService> &remotingService,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    std::shared_ptr<LocalMachine> localMachine,
    uv_async_t *initComplete);

tempo_utils::Status make_custom_channel(
    std::shared_ptr<grpc::ChannelInterface> &channel,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig);

tempo_utils::Status make_invoke_service_stub(
    std::unique_ptr<chord_invoke::InvokeService::StubInterface> &stub,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    std::shared_ptr<grpc::ChannelInterface> &customChannel);

tempo_utils::Status make_csr_key_pair(
    tempo_security::CSRKeyPair &csrKeyPair,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig);

tempo_utils::Status make_grpc_binder(
    std::shared_ptr<GrpcBinder> &binder,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    const tempo_security::CSRKeyPair &csrKeyPair,
    chord_invoke::InvokeService::StubInterface *invokeStub,
    RemotingService *remotingService);

#endif // CHORD_LOCAL_MACHINE_INITIALIZE_UTILS_H
