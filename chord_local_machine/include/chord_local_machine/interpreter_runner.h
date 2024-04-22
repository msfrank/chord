#ifndef CHORD_LOCAL_MACHINE_INTERPRETER_THREAD_H
#define CHORD_LOCAL_MACHINE_INTERPRETER_THREAD_H

#include <uv.h>

#include <lyric_runtime/bytecode_interpreter.h>
#include <tempo_utils/status.h>

#include "abstract_message_sender.h"
#include "async_queue.h"

struct RunnerRequest : public AbstractMessage {
    enum class MessageType {
        Start,
        Interrupt,
        Shutdown,
    };
    MessageType type;
    explicit RunnerRequest(MessageType type): type(type) {};
};

struct StartRunner : public RunnerRequest {
    StartRunner() : RunnerRequest(RunnerRequest::MessageType::Start) {};
};

struct InterruptRunner : public RunnerRequest {
    InterruptRunner() : RunnerRequest(RunnerRequest::MessageType::Interrupt) {};
};

struct ShutdownRunner : public RunnerRequest {
    ShutdownRunner() : RunnerRequest(RunnerRequest::MessageType::Shutdown) {};
};

struct RunnerReply : public AbstractMessage {
    enum class MessageType {
        Running,
        Suspended,
        Completed,
        Cancelled,
        Failure,
    };
    MessageType type;
    explicit RunnerReply(MessageType type): type(type) {};
};

struct RunnerRunning : public RunnerReply {
    RunnerRunning()
        : RunnerReply(RunnerReply::MessageType::Running)
    {};
};

struct RunnerSuspended : public RunnerReply {
    RunnerSuspended()
        : RunnerReply(RunnerReply::MessageType::Suspended)
    {};
};

struct RunnerCancelled : public RunnerReply {
    RunnerCancelled()
        : RunnerReply(RunnerReply::MessageType::Cancelled)
    {};
};

struct RunnerCompleted : public RunnerReply {
    explicit RunnerCompleted()
        : RunnerReply(RunnerReply::MessageType::Completed)
    {};
};

struct RunnerFailure : public RunnerReply {
    tempo_utils::Status status;
    explicit RunnerFailure(const tempo_utils::Status &status)
        : RunnerReply(RunnerReply::MessageType::Failure),
          status(status)
    {};
};

enum class InterpreterRunnerState {
    INITIAL,
    RUNNING,
    STOPPED,
    SHUTDOWN,
    FAILED,
};

class InterpreterRunner {
public:
    InterpreterRunner(
        std::unique_ptr<lyric_runtime::BytecodeInterpreter> interp,
        AbstractMessageSender<RunnerReply> *outgoing);

    AbstractMessageSender<RunnerRequest> *getIncomingSender() const;
    InterpreterRunnerState getState() const;
    tempo_utils::Status getStatus() const;
    lyric_runtime::Return getResult() const;

    tempo_utils::Status run();

private:
    std::unique_ptr<lyric_runtime::BytecodeInterpreter> m_interp;
    AbstractMessageSender<RunnerReply> *m_outgoing;
    std::unique_ptr<AsyncQueue<RunnerRequest>> m_incoming;

    std::unique_ptr<absl::Mutex> m_lock;
    InterpreterRunnerState m_state ABSL_GUARDED_BY(m_lock);
    tempo_utils::Status m_status ABSL_GUARDED_BY(m_lock);
    lyric_runtime::Return m_result ABSL_GUARDED_BY(m_lock);

    void beforeRunInterpreter();
    void runInterpreter();
    void suspendInterpreter();
    void shutdownInterpreter();
};

#endif // CHORD_LOCAL_MACHINE_INTERPRETER_THREAD_H
