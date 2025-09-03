
#include <chord_local_machine/grpc_binder.h>
#include <chord_local_machine/local_machine.h>
#include <chord_local_machine/run_protocol_socket.h>
#include <tempo_command/command_result.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/log_sink.h>
#include <tempo_utils/log_stream.h>
#include <tempo_utils/posix_result.h>
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
    chordLocalMachineData->localMachine->notifyInitComplete();
}

static bool
on_runner_reply(RunnerReply *message, void *data)
{
    auto *chordLocalMachineData = static_cast<ChordLocalMachineData *>(data);
    chord_remoting::MachineState newState = chord_remoting::UnknownState;
    tempo_utils::StatusCode statusCode = tempo_utils::StatusCode::kUnknown;
    bool stopProcessor = false;

    switch (message->type) {
        case RunnerReply::MessageType::Running:
            newState = chord_remoting::Running;
            stopProcessor = false;
            break;
        case RunnerReply::MessageType::Suspended:
            newState = chord_remoting::Suspended;
            stopProcessor = false;
            break;
        case RunnerReply::MessageType::Cancelled:
            newState = chord_remoting::Cancelled;
            statusCode = tempo_utils::StatusCode::kCancelled;
            stopProcessor = true;
            break;
        case RunnerReply::MessageType::Completed:
            newState = chord_remoting::Completed;
            statusCode = static_cast<RunnerCompleted *>(message)->statusCode;
            stopProcessor = true;
            break;
        case RunnerReply::MessageType::Failure:
            newState = chord_remoting::Failure;
            statusCode = static_cast<RunnerFailure *>(message)->status.getStatusCode();
            stopProcessor = true;
            break;
    }

    delete message;

    if (stopProcessor) {
        chordLocalMachineData->remotingService->notifyMachineExit(statusCode);
    }
    chordLocalMachineData->remotingService->notifyMachineStateChanged(newState);
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
        auto logSink = std::make_unique<tempo_utils::LogFileSink>(chordLocalMachineConfig.logFile);
        tempo_utils::init_logging(loggingConfig, std::move(logSink));
    } else {
        tempo_utils::init_logging(loggingConfig);
    }

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

    // allocate the remoting service
    TU_RETURN_IF_NOT_OK (make_remoting_service(chordLocalMachineData.remotingService,
        componentConstructor, chordLocalMachineConfig, chordLocalMachineData.localMachine, &initComplete));

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
    auto shutdownMachineStatus = chordLocalMachineData.localMachine->terminate();
    TU_LOG_WARN_IF (shutdownMachineStatus.notOk()) << "failed to shut down local machine: " << shutdownMachineStatus;
    chordLocalMachineData.localMachine.reset();

    // flush any remaining RunnerReply messages
    processor.processAvailableMessages();

    //
    uv_print_all_handles(&chordLocalMachineData.mainLoop, stderr);

    // shut down the binder
    TU_LOG_V << "shutting down grpc binder";
    auto shutdownBinderStatus = chordLocalMachineData.grpcBinder->shutdown();
    TU_LOG_WARN_IF (shutdownBinderStatus.notOk()) << "failed to shut down grpc binder: " << shutdownBinderStatus;
    chordLocalMachineData.grpcBinder.reset();

//    // release the run socket
//    TU_LOG_V << "releasing run socket";
//    chordLocalMachineData.runSocket.reset();

    // release the remoting service
    TU_LOG_V << "releasing remoting service";
    chordLocalMachineData.remotingService.reset();

    // release the invoke stub
    TU_LOG_V << "releasing invoke stub";
    chordLocalMachineData.invokeStub.reset();

    return runStatus;
}