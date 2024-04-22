#ifndef CHORD_LOCAL_MACHINE_LOCAL_MACHINE_H
#define CHORD_LOCAL_MACHINE_LOCAL_MACHINE_H

#include <absl/synchronization/mutex.h>
#include <absl/synchronization/notification.h>
#include <uv.h>

#include <lyric_runtime/bytecode_interpreter.h>

#include "interpreter_runner.h"

class LocalMachine {

public:
    LocalMachine(
        const tempo_utils::Url &machineUrl,
        std::shared_ptr<lyric_runtime::InterpreterState> interpreterState,
        AbstractMessageSender<RunnerReply> *processor);
    virtual ~LocalMachine();

    tempo_utils::Url getMachineUrl() const;
    InterpreterRunnerState getRunnerState() const;

    tempo_utils::Status start();
    tempo_utils::Status stop();
    tempo_utils::Status shutdown();

private:
    tempo_utils::Url m_machineUrl;
    std::unique_ptr<InterpreterRunner> m_runner;
    AbstractMessageSender<RunnerRequest> *m_commandQueue;
    uv_thread_t m_tid;
};

#endif // CHORD_LOCAL_MACHINE_LOCAL_MACHINE_H