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
        bool startSuspended,
        std::shared_ptr<lyric_runtime::InterpreterState> interpreterState,
        AbstractMessageSender<RunnerReply> *processor);
    virtual ~LocalMachine();

    tempo_utils::Url getMachineUrl() const;
    InterpreterRunnerState getRunnerState() const;

    tempo_utils::Status notifyInitComplete();
    tempo_utils::Status suspend();
    tempo_utils::Status resume();
    tempo_utils::Status terminate();

private:
    tempo_utils::Url m_machineUrl;
    bool m_startSuspended;
    std::unique_ptr<InterpreterRunner> m_runner;
    AbstractMessageSender<RunnerRequest> *m_commandQueue;
    uv_thread_t m_tid;
};

#endif // CHORD_LOCAL_MACHINE_LOCAL_MACHINE_H