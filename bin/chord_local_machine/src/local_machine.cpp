
#include <chord_local_machine/local_machine.h>

static void runner_thread(void *data)
{
    auto *runner = static_cast<InterpreterRunner *>(data);
    TU_ASSERT (runner != nullptr);
    auto status = runner->run();
    if (status.notOk()) {
        TU_LOG_ERROR << "local machine failed";
    } else {
        TU_LOG_V << "local machine completed";
    }
}

LocalMachine::LocalMachine(
    const tempo_utils::Url &machineUrl,
    bool startSuspended,
    std::shared_ptr<lyric_runtime::InterpreterState> interpreterState,
    AbstractMessageSender<RunnerReply> *processor)
    : m_machineUrl(machineUrl),
      m_startSuspended(startSuspended)
{
    TU_ASSERT (m_machineUrl.isValid());
    TU_ASSERT (interpreterState != nullptr);
    TU_ASSERT (processor != nullptr);

    auto interp = std::make_unique<lyric_runtime::BytecodeInterpreter>(interpreterState, nullptr);
    m_runner = std::make_unique<InterpreterRunner>(std::move(interp), processor);
    m_commandQueue = m_runner->getIncomingSender();
    uv_thread_create(&m_tid, runner_thread, m_runner.get());
}

LocalMachine::~LocalMachine()
{
    terminate();
    uv_thread_join(&m_tid);
}

tempo_utils::Url
LocalMachine::getMachineUrl() const
{
    return m_machineUrl;
}

InterpreterRunnerState
LocalMachine::getRunnerState() const
{
    return m_runner->getState();
}

tempo_utils::Status
LocalMachine::notifyInitComplete()
{
    if (!m_startSuspended) {
        resume();
    }
    return {};
}

tempo_utils::Status
LocalMachine::suspend()
{
    m_commandQueue->sendMessage(new SuspendRunner());
    return {};
}

tempo_utils::Status
LocalMachine::resume()
{
    m_commandQueue->sendMessage(new ResumeRunner());
    return {};
}

tempo_utils::Status
LocalMachine::terminate()
{
    m_commandQueue->sendMessage(new TerminateRunner());
    return {};
}