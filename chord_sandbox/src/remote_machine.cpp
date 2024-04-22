
#include <chord_sandbox/remote_machine.h>

chord_sandbox::RemoteMachine::RemoteMachine(
    std::string_view name,
    const lyric_common::AssemblyLocation &mainLocation,
    const tempo_utils::Url &machineUrl,
    std::shared_ptr<GrpcConnector> connector,
    std::shared_ptr<RunProtocolPlug> runPlug)
    : m_name(name),
      m_mainLocation(mainLocation),
      m_machineUrl(machineUrl),
      m_connector(connector),
      m_runPlug(runPlug)
{
    TU_ASSERT (!m_name.empty());
    TU_ASSERT (m_mainLocation.isValid());
    TU_ASSERT (m_machineUrl.isValid());
    TU_ASSERT (m_connector != nullptr);
    TU_ASSERT (m_runPlug != nullptr);
}

tempo_utils::Status
chord_sandbox::RemoteMachine::runUntilFinished()
{
    absl::MutexLock locker(&m_lock);
    auto state = m_runPlug->getState();
    while (state != RunPlugState::FINISHED) {
        auto waitResult = m_runPlug->waitForStateChange(state);
        if (waitResult.isStatus())
            return waitResult.getStatus();
        state = waitResult.getResult();
    }
    return SandboxStatus::ok();
}

tempo_utils::Status
chord_sandbox::RemoteMachine::start()
{
    absl::MutexLock locker(&m_lock);
    return m_connector->start();
}

tempo_utils::Status
chord_sandbox::RemoteMachine::stop()
{
    absl::MutexLock locker(&m_lock);
    m_connector->stop();
    return SandboxStatus::ok();
}

tempo_utils::Status
chord_sandbox::RemoteMachine::shutdown()
{
    absl::MutexLock locker(&m_lock);
    return SandboxStatus::forCondition(
        SandboxCondition::kSandboxInvariant, "shutdown is unimplemented");
}