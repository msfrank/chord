
#include <chord_sandbox/remote_machine.h>

chord_sandbox::RemoteMachine::RemoteMachine(
    std::string_view name,
    const tempo_utils::Url &mainLocation,
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

/**
 * Block the current thread until the remote machine is finished. If the given `func` is not null then
 * it will be called every time there is a state change, and `data` will be passed as the second argument
 * to `func`.
 *
 * @param func Callback function invoked every time there is a state change.
 * @param data void pointer to data which is passed to `func`.
 * @return Ok status if the remote machine finished, otherwise non-Ok status containing an error.
 */
tempo_utils::Result<chord_sandbox::MachineExit>
chord_sandbox::RemoteMachine::runUntilFinished(RemoteMachineStateChangedFunc func, void *data)
{
    auto monitor = m_connector->getMonitor();
    auto state = monitor->getState();

    // loop until we encounter a terminal state
    for (;;) {

        // if there is a callback func specified then call it
        if (func != nullptr) {
            func(state, data);
        }

        // if state is terminal then break from the loop
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

        // otherwise block waiting for a state change
        state = monitor->waitForStateChange(state, -1);
    }

    MachineExit machineExit;
    machineExit.statusCode = monitor->getStatusCode();
    return machineExit;
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
    return m_connector->terminate();
}