#ifndef CHORD_MACHINE_LOCAL_MACHINE_H
#define CHORD_MACHINE_LOCAL_MACHINE_H

#include <absl/synchronization/mutex.h>
#include <absl/synchronization/notification.h>
#include <uv.h>

#include <lyric_runtime/bytecode_interpreter.h>

#include "interpreter_runner.h"

namespace chord_machine {

    class LocalMachine {
    public:
        LocalMachine(
            const std::string &machineName,
            bool startSuspended,
            std::shared_ptr<lyric_runtime::InterpreterState> interpreterState,
            AbstractMessageSender<RunnerReply> *processor);
        virtual ~LocalMachine();

        std::string getMachineName() const;
        InterpreterRunnerState getRunnerState() const;

        tempo_utils::Status notifyInitComplete();
        tempo_utils::Status suspend();
        tempo_utils::Status resume();
        tempo_utils::Status terminate();

    private:
        std::string m_machineName;
        bool m_startSuspended;
        std::unique_ptr<InterpreterRunner> m_runner;
        AbstractMessageSender<RunnerRequest> *m_commandQueue;
        uv_thread_t m_tid;
    };
}

#endif // CHORD_MACHINE_LOCAL_MACHINE_H