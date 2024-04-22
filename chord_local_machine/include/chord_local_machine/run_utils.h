#ifndef CHORD_LOCAL_MACHINE_RUN_UTILS_H
#define CHORD_LOCAL_MACHINE_RUN_UTILS_H

#include <absl/container/flat_hash_set.h>
#include <uv.h>

#include <chord_invoke/invoke_service.grpc.pb.h>
#include <lyric_runtime/interpreter_state.h>
#include <tempo_security/csr_key_pair.h>
#include <tempo_utils/status.h>
#include <tempo_utils/url.h>

#include "grpc_binder.h"
#include "local_machine.h"
#include "remoting_service.h"
#include "run_protocol_socket.h"
#include "config_utils.h"

struct ChordLocalMachineData {
    int signum = -1;
    uv_loop_t mainLoop;
    std::unique_ptr<RemotingService> remotingService;
    std::shared_ptr<grpc::ChannelInterface> customChannel;
    std::unique_ptr<chord_invoke::InvokeService::StubInterface> invokeStub;
    std::shared_ptr<lyric_runtime::InterpreterState> interpreterState;
    std::shared_ptr<LocalMachine> localMachine;
    std::shared_ptr<RunProtocolSocket> runSocket;
    tempo_security::CSRKeyPair csrKeyPair;
    std::filesystem::path pemCertificateFile;
    std::shared_ptr<GrpcBinder> grpcBinder;
};

tempo_utils::Status sign_certificates(
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    ChordLocalMachineData &chordLocalMachineData);

tempo_utils::Status register_protocols(
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    ChordLocalMachineData &chordLocalMachineData);

tempo_utils::Status advertise_endpoints(
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    ChordLocalMachineData &chordLocalMachineData);

tempo_utils::Status run_local_machine(
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    ChordLocalMachineData &chordLocalMachineData);

#endif // CHORD_LOCAL_MACHINE_RUN_UTILS_H
