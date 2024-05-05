
#include <chord_local_machine/interpreter_runner.h>

InterpreterRunner::InterpreterRunner(
    std::unique_ptr<lyric_runtime::BytecodeInterpreter> interp,
    AbstractMessageSender<RunnerReply> *outgoing)
    : m_interp(std::move(interp)),
      m_outgoing(outgoing),
      m_incoming(std::make_unique<AsyncQueue<RunnerRequest>>()),
      m_lock(std::make_unique<absl::Mutex>()),
      m_state(InterpreterRunnerState::INITIAL)
{
    TU_ASSERT (m_interp != nullptr);
    TU_ASSERT (m_outgoing != nullptr);
}

AbstractMessageSender<RunnerRequest> *
InterpreterRunner::getIncomingSender() const
{
    absl::MutexLock locker(m_lock.get());
    return m_incoming.get();
}

InterpreterRunnerState
InterpreterRunner::getState() const
{
    absl::MutexLock locker(m_lock.get());
    return m_state;
}

tempo_utils::Status
InterpreterRunner::getStatus() const
{
    absl::MutexLock locker(m_lock.get());
    return m_status;
}

lyric_runtime::Return
InterpreterRunner::getResult() const
{
    absl::MutexLock locker(m_lock.get());
    return m_result;
}

tempo_utils::Status
InterpreterRunner::run()
{
    auto *state = m_interp->interpreterState();
    auto *loop = state->mainLoop();

    TU_RETURN_IF_NOT_OK (m_incoming->initialize(loop));

    for (;;) {

        TU_LOG_V << "waiting for runner request";
        std::unique_ptr<RunnerRequest> request(m_incoming->waitForMessage());
        if (request == nullptr)
            continue;

        switch (request->type) {

            case RunnerRequest::MessageType::Resume: {
                beforeRunInterpreter();
                runInterpreter();
                break;
            }

            case RunnerRequest::MessageType::Suspend: {
                suspendInterpreter();
                break;
            }

            case RunnerRequest::MessageType::Terminate: {
                shutdownInterpreter();
                return m_status;
            }

            default:
                TU_LOG_WARN << "ignoring unknown message " << request.get();
                break;
        }
    }
}

void
InterpreterRunner::beforeRunInterpreter()
{
    TU_LOG_V << "beforeRunInterpreter";

    absl::MutexLock locker(m_lock.get());

    switch (m_state) {
        case InterpreterRunnerState::RUNNING:
            break;

        case InterpreterRunnerState::INITIAL:
        case InterpreterRunnerState::STOPPED:
            m_outgoing->sendMessage(new RunnerRunning());
            break;

        case InterpreterRunnerState::SHUTDOWN:
            m_outgoing->sendMessage(new RunnerCompleted());
            return;

        case InterpreterRunnerState::FAILED:
            m_outgoing->sendMessage(new RunnerFailure(m_status));
            return;

        default:
            m_outgoing->sendMessage(new RunnerFailure(
                lyric_runtime::InterpreterStatus::forCondition(
                    lyric_runtime::InterpreterCondition::kRuntimeInvariant,
                    "failed to run interpreter: unexpected interpreter state")));
            m_state = InterpreterRunnerState::FAILED;
            return;
    }

    m_state = InterpreterRunnerState::RUNNING;
}

void
InterpreterRunner::runInterpreter()
{
    TU_LOG_V << "runInterpreter";

    auto runInterpResult = m_interp->run();

    absl::MutexLock locker(m_lock.get());

    if (runInterpResult.isStatus()) {
        m_status = runInterpResult.getStatus();
        if (!m_status.matchesCondition(lyric_runtime::InterpreterCondition::kInterrupted)) {
            TU_LOG_V << "interpreter suspended";
            m_outgoing->sendMessage(new RunnerSuspended());
            m_state = InterpreterRunnerState::STOPPED;
        } else {
            TU_LOG_ERROR << "interpreter failed: " << m_status;
            m_outgoing->sendMessage(new RunnerFailure(m_status));
            m_state = InterpreterRunnerState::FAILED;
        }
        return;
    }

    TU_LOG_V << "interpreter completed";
    m_result = runInterpResult.getResult();
    m_outgoing->sendMessage(new RunnerCompleted());
    m_state = InterpreterRunnerState::SHUTDOWN;
}

void
InterpreterRunner::suspendInterpreter()
{
    TU_LOG_V << "suspendInterpreter";

    absl::MutexLock locker(m_lock.get());

    switch (m_state) {
        case InterpreterRunnerState::INITIAL:
        case InterpreterRunnerState::RUNNING:
        case InterpreterRunnerState::STOPPED:
            m_outgoing->sendMessage(new RunnerSuspended());
            m_state = InterpreterRunnerState::STOPPED;
            return;

        case InterpreterRunnerState::FAILED:
            m_outgoing->sendMessage(new RunnerFailure(m_status));
            return;

        case InterpreterRunnerState::SHUTDOWN:
        default:
            m_outgoing->sendMessage(new RunnerFailure(
                lyric_runtime::InterpreterStatus::forCondition(
                    lyric_runtime::InterpreterCondition::kRuntimeInvariant,
                    "failed to run interpreter: unexpected interpreter state")));
            m_state = InterpreterRunnerState::FAILED;
            return;
    }
}

void
InterpreterRunner::shutdownInterpreter()
{
    TU_LOG_V << "shutdownInterpreter";

    absl::MutexLock locker(m_lock.get());

    switch (m_state) {
        case InterpreterRunnerState::INITIAL:
        case InterpreterRunnerState::RUNNING:
        case InterpreterRunnerState::STOPPED:
            m_outgoing->sendMessage(new RunnerCancelled());
            m_state = InterpreterRunnerState::SHUTDOWN;
            return;

        case InterpreterRunnerState::SHUTDOWN:
            m_outgoing->sendMessage(new RunnerCompleted());
            return;

        case InterpreterRunnerState::FAILED:
            m_outgoing->sendMessage(new RunnerFailure(m_status));
            return;

        default:
            m_outgoing->sendMessage(new RunnerFailure(
                lyric_runtime::InterpreterStatus::forCondition(
                    lyric_runtime::InterpreterCondition::kRuntimeInvariant,
                    "failed to run interpreter: unexpected interpreter state")));
            m_state = InterpreterRunnerState::FAILED;
            return;
    }
}
