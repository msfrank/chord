
#include <chord_sandbox/remote_machine.h>

chord_sandbox::RemoteMachine::RemoteMachine(
    std::string_view name,
    const lyric_common::AssemblyLocation &mainLocation,
    const tempo_utils::Url &machineUrl,
    std::shared_ptr<GrpcConnector> connector)
    : m_name(name),
      m_mainLocation(mainLocation),
      m_machineUrl(machineUrl),
      m_connector(connector)
{
    TU_ASSERT (!m_name.empty());
    TU_ASSERT (m_mainLocation.isValid());
    TU_ASSERT (m_machineUrl.isValid());
    TU_ASSERT (m_connector != nullptr);
}

tempo_utils::Status
chord_sandbox::RemoteMachine::runUntilFinished()
{
    auto monitor = m_connector->getMonitor();
    auto state = monitor->getState();
    TU_LOG_INFO << "initial state: " << chord_remoting::MachineState_Name(state);

    for (;;) {
        bool done;
        switch (state) {
            case chord_remoting::Completed:
            case chord_remoting::Cancelled:
            case chord_remoting::Failure:
                done = true;
                break;
            default:
                done = false;
                break;
        }
        if (done)
            break;

        state = monitor->waitForStateChange(state, -1);
        TU_LOG_INFO << "state changed: " << chord_remoting::MachineState_Name(state);
    }

    return {};
}

tempo_utils::Status
chord_sandbox::RemoteMachine::suspend()
{
    return m_connector->suspend();
}

tempo_utils::Status
chord_sandbox::RemoteMachine::resume()
{
    return m_connector->resume();
}

tempo_utils::Status
chord_sandbox::RemoteMachine::shutdown()
{
    absl::MutexLock locker(&m_lock);
    return SandboxStatus::forCondition(
        SandboxCondition::kSandboxInvariant, "shutdown is unimplemented");
}