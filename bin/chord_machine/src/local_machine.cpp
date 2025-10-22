
#include <chord_machine/local_machine.h>

static void runner_thread(void *data)
{
    auto *runner = static_cast<chord_machine::InterpreterRunner *>(data);
    TU_ASSERT (runner != nullptr);
    auto status = runner->run();
    if (status.notOk()) {
        TU_LOG_ERROR << "local machine failed";
    } else {
        TU_LOG_V << "local machine completed";
    }
}

chord_machine::LocalMachine::LocalMachine(
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

chord_machine::LocalMachine::~LocalMachine()
{
    terminate();
    uv_thread_join(&m_tid);
}

tempo_utils::Url
chord_machine::LocalMachine::getMachineUrl() const
{
    return m_machineUrl;
}

chord_machine::InterpreterRunnerState
chord_machine::LocalMachine::getRunnerState() const
{
    return m_runner->getState();
}

tempo_utils::Status
chord_machine::LocalMachine::notifyInitComplete()
{
    if (!m_startSuspended) {
        resume();
    }
    return {};
}

tempo_utils::Status
chord_machine::LocalMachine::suspend()
{
    m_commandQueue->sendMessage(new SuspendRunner());
    return {};
}

tempo_utils::Status
chord_machine::LocalMachine::resume()
{
    m_commandQueue->sendMessage(new ResumeRunner());
    return {};
}

tempo_utils::Status
chord_machine::LocalMachine::terminate()
{
    m_commandQueue->sendMessage(new TerminateRunner());
    return {};
}