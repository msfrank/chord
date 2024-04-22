
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
    std::shared_ptr<lyric_runtime::InterpreterState> interpreterState,
    AbstractMessageSender<RunnerReply> *processor)
    : m_machineUrl(machineUrl)
{
    TU_ASSERT (m_machineUrl.isValid());
    auto interp = std::make_unique<lyric_runtime::BytecodeInterpreter>(interpreterState, nullptr);
    m_runner = std::make_unique<InterpreterRunner>(std::move(interp), processor);
    m_commandQueue = m_runner->getIncomingSender();
    uv_thread_create(&m_tid, runner_thread, m_runner.get());
}

LocalMachine::~LocalMachine()
{
    shutdown();
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
LocalMachine::start()
{
    m_commandQueue->sendMessage(new StartRunner());
    return {};
}

tempo_utils::Status
LocalMachine::stop()
{
    m_commandQueue->sendMessage(new InterruptRunner());
    return {};
}

tempo_utils::Status
LocalMachine::shutdown()
{
    m_commandQueue->sendMessage(new ShutdownRunner());
    return {};
}