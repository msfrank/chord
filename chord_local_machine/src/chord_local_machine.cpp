#include <sys/socket.h>
#include <sys/un.h>

#include <chord_invoke/invoke_service.grpc.pb.h>
#include <chord_local_machine/grpc_binder.h>
#include <chord_local_machine/local_machine.h>
#include <chord_local_machine/port_socket.h>
#include <chord_local_machine/run_protocol_socket.h>
#include <lyric_packaging/directory_loader.h>
#include <lyric_packaging/package_loader.h>
#include <lyric_common/common_conversions.h>
#include <lyric_runtime/chain_loader.h>
#include <lyric_runtime/interpreter_state.h>
#include <tempo_command/command_result.h>
#include <tempo_security/ecc_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_utils/file_reader.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/log_stream.h>
#include <tempo_utils/posix_result.h>
#include <tempo_utils/tempfile_maker.h>
#include <tempo_utils/url.h>

#include <chord_local_machine/async_processor.h>
#include <chord_local_machine/config_utils.h>
#include <chord_local_machine/initialize_utils.h>
#include <chord_local_machine/run_utils.h>
#include <chord_local_machine/chord_local_machine.h>

static void
on_signal(uv_signal_t *handle, int signum)
{
    switch (signum) {
        case SIGTERM:
            TU_LOG_INFO << "caught SIGTERM signal";
            break;
        case SIGINT:
            TU_LOG_INFO << "caught SIGINT signal";
            break;
        default:
            TU_LOG_INFO << "caught signal " << signum;
            break;
    }
    auto *chordLocalMachineData = (ChordLocalMachineData *) handle->loop->data;
    chordLocalMachineData->signum = signum;
    uv_stop(handle->loop);
}

static void
on_init_complete(uv_async_t *handle)
{
    TU_LOG_INFO << "launch protocols connected";
    auto *chordLocalMachineData = (ChordLocalMachineData *) handle->loop->data;
    if (chordLocalMachineData->runSocket) {
        chordLocalMachineData->runSocket->notifyInitComplete();
    } else {
        chordLocalMachineData->localMachine->start();
    }
}

//static void
//on_machine_state_changed(uv_async_t *handle)
//{
//    TU_LOG_INFO << "detected interpreter state change";
//    auto *runSocket = static_cast<RunProtocolSocket *>(handle->data);
//    runSocket->notifyStateChanged();
//}

static bool
on_runner_reply(RunnerReply *message, void *data)
{
    auto *chordLocalMachineData = static_cast<ChordLocalMachineData *>(data);
    bool stopProcessor;

    if (chordLocalMachineData->runSocket) {
        chordLocalMachineData->runSocket->notifyStateChanged();
    }

    switch (message->type) {
        case RunnerReply::MessageType::Cancelled:
        case RunnerReply::MessageType::Completed:
        case RunnerReply::MessageType::Failure:
            stopProcessor = true;
            break;
        default:
            stopProcessor = false;
            break;
    }

    delete message;
    return stopProcessor;
}

tempo_utils::Status
run_chord_local_machine(int argc, const char *argv[])
{
    ChordLocalMachineConfig chordLocalMachineConfig;
    TU_RETURN_IF_NOT_OK (configure(chordLocalMachineConfig, argc, argv));

    // set the current working directory to the run directory
    std::error_code ec;
    current_path(chordLocalMachineConfig.runDirectory, ec);

    // initialize logging
    tempo_utils::LoggingConfiguration loggingConfig;
    loggingConfig.severityFilter = tempo_utils::SeverityFilter::kVeryVerbose;
    loggingConfig.flushEveryMessage = true;
    if (!chordLocalMachineConfig.logFile.empty()) {
        auto *logFileString = chordLocalMachineConfig.logFile.c_str();
        auto *logFp = std::fopen(logFileString, "a");
        if (logFp == nullptr) {
            return tempo_utils::PosixStatus::last(
                absl::StrCat("failed to open logfile ", logFileString));
        }
        loggingConfig.logFile = logFp;
    }
    tempo_utils::init_logging(loggingConfig);

    //
    ChordLocalMachineData chordLocalMachineData;
    ComponentConstructor componentConstructor;

    // initialize the main loop
    auto ret = uv_loop_init(&chordLocalMachineData.mainLoop);
    if (ret != 0)
        return tempo_command::CommandStatus::forCondition(
            tempo_command::CommandCondition::kCommandError, "uv loop initialization failure");
    chordLocalMachineData.mainLoop.data = &chordLocalMachineData;

    uv_async_t initComplete;
    uv_async_init(&chordLocalMachineData.mainLoop, &initComplete, on_init_complete);

    // add signal watchers for SIGTERM and SIGINT
    uv_signal_t sigterm;
    uv_signal_init(&chordLocalMachineData.mainLoop, &sigterm);
    uv_signal_start(&sigterm, on_signal, SIGTERM);
    uv_signal_t sigint;
    uv_signal_init(&chordLocalMachineData.mainLoop, &sigint);
    uv_signal_start(&sigint, on_signal, SIGINT);

    TU_LOG_V << "initialized main loop";

    AsyncProcessor<RunnerReply> processor(on_runner_reply, &chordLocalMachineData);
    TU_RETURN_IF_NOT_OK (processor.initialize(&chordLocalMachineData.mainLoop));

    // allocate the remoting service
    chordLocalMachineData.remotingService = std::make_unique<RemotingService>(&initComplete);

    // construct the invoke service stub
    TU_RETURN_IF_NOT_OK (make_custom_channel(
        chordLocalMachineData.customChannel, componentConstructor, chordLocalMachineConfig));

    // construct the invoke service stub
    TU_RETURN_IF_NOT_OK (make_invoke_service_stub(
        chordLocalMachineData.invokeStub, componentConstructor, chordLocalMachineConfig,
        chordLocalMachineData.customChannel));

    // construct the interpreter state
    TU_RETURN_IF_NOT_OK (make_interpreter_state(
        chordLocalMachineData.interpreterState, componentConstructor, chordLocalMachineConfig));

    // construct the local machine
    TU_RETURN_IF_NOT_OK (make_local_machine(chordLocalMachineData.localMachine,
        componentConstructor, chordLocalMachineConfig, chordLocalMachineData.interpreterState, &processor));

    // allocate the run socket if run protocol is an expected port
    if (chordLocalMachineConfig.expectedPorts.contains(tempo_utils::Url::fromString(kRunProtocolUri))) {
        chordLocalMachineData.runSocket = std::make_shared<RunProtocolSocket>(chordLocalMachineData.localMachine);
    }

    // construct the certificate signing request
    TU_RETURN_IF_NOT_OK (make_csr_key_pair(
        chordLocalMachineData.csrKeyPair, componentConstructor, chordLocalMachineConfig));

    // construct the grpc binder
    TU_RETURN_IF_NOT_OK (make_grpc_binder(
        chordLocalMachineData.grpcBinder, componentConstructor, chordLocalMachineConfig,
        chordLocalMachineData.csrKeyPair, chordLocalMachineData.invokeStub.get(),
        chordLocalMachineData.remotingService.get()));


    // run the local machine
    auto runStatus = run_local_machine(chordLocalMachineConfig, chordLocalMachineData);
    TU_LOG_WARN_IF (runStatus.notOk()) << runStatus;

    // shut down the local machine
    TU_LOG_V << "shutting down local machine";
    auto shutdownMachineStatus = chordLocalMachineData.localMachine->shutdown();
    TU_LOG_WARN_IF (shutdownMachineStatus.notOk()) << "failed to shutdown local machine: " << shutdownMachineStatus;
    chordLocalMachineData.localMachine.reset();

    uv_print_all_handles(&chordLocalMachineData.mainLoop, tempo_utils::get_logging_sink());

    // shut down the binder
    TU_LOG_V << "shutting down grpc binder";
    auto shutdownBinderStatus = chordLocalMachineData.grpcBinder->shutdown();
    TU_LOG_WARN_IF (shutdownBinderStatus.notOk()) << "failed to shutdown grpc binder: " << shutdownBinderStatus;
    chordLocalMachineData.grpcBinder.reset();

    // release the run socket
    TU_LOG_V << "releasing run socket";
    chordLocalMachineData.runSocket.reset();

    // release the remoting service
    TU_LOG_V << "releasing remoting service";
    chordLocalMachineData.remotingService.reset();

    // release the invoke stub
    TU_LOG_V << "releasing invoke stub";
    chordLocalMachineData.invokeStub.reset();

    return runStatus;
}