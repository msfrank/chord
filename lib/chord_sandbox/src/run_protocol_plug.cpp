
#include <chord_sandbox/run_protocol_plug.h>
#include <chord_sandbox/sandbox_result.h>

chord_sandbox::RunProtocolPlug::RunProtocolPlug(chord_sandbox::RunProtocolCallback cb, void *data)
    : m_cb(cb),
      m_data(data),
      m_state(RunPlugState::READY),
      m_writer(nullptr)
{
    TU_ASSERT(m_cb != nullptr);
}

bool
chord_sandbox::RunProtocolPlug::isAttached()
{
    absl::MutexLock locker(&m_lock);
    return m_writer != nullptr;
}

tempo_utils::Status
chord_sandbox::RunProtocolPlug::attach(chord_common::AbstractProtocolWriter *writer)
{
    m_lock.Lock();
    m_writer = writer;
    TU_LOG_INFO << "attached RunProtocolPlug";
    m_state = RunPlugState::STARTING;
    m_cond.Signal();
    m_lock.Unlock();

    m_cb(RunPlugState::STARTING, m_data);
    return send("START");
}

tempo_utils::Status
chord_sandbox::RunProtocolPlug::send(std::string_view message)
{
    absl::MutexLock locker(&m_lock);
    TU_LOG_INFO << "sending message: " << std::string(message);
    return m_writer->write(message);
}

tempo_utils::Status
chord_sandbox::RunProtocolPlug::handle(std::string_view message)
{
    m_lock.Lock();

    // start critical section
    TU_LOG_INFO << "received message: " << std::string(message);
    tempo_utils::Status status;
    if (message == "OK") {
        switch (m_state) {
            case RunPlugState::STARTING:
                m_state = RunPlugState::RUNNING;
                break;
            case RunPlugState::STOPPING:
                m_state = RunPlugState::STOPPED;
                break;
            default:
                m_state = RunPlugState::INVALID;
                status = SandboxStatus::forCondition(
                    SandboxCondition::kSandboxInvariant, "invalid run plug state");
                break;
        }
        m_cond.Signal();
    }
    else if (message == "FINISHED") {
        m_state = RunPlugState::FINISHED;
        m_cond.Signal();
    } else {
        status = SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "unknown run message");
    }
    auto state = m_state;
    // end critical section

    m_lock.Unlock();

    m_cb(state, m_data);
    return status;
}

tempo_utils::Status
chord_sandbox::RunProtocolPlug::detach()
{
    m_lock.Lock();
    m_writer = nullptr;
    TU_LOG_INFO << "detached RunProtocolPlug";
    auto prev = m_state;
    m_state = RunPlugState::FINISHED;
    if (m_state != prev) {
        m_cond.Signal();
    }
    m_lock.Unlock();

    if (prev != RunPlugState::FINISHED) {
        m_cb(RunPlugState::STARTING, m_data);
    }

    return {};
}

chord_sandbox::RunPlugState
chord_sandbox::RunProtocolPlug::getState()
{
    absl::MutexLock locker(&m_lock);
    return m_state;
}

tempo_utils::Result<chord_sandbox::RunPlugState>
chord_sandbox::RunProtocolPlug::waitForStateChange(RunPlugState prev, int timeout)
{
    m_lock.Lock();

    // if state is FINISHED then return immediately because there will be no more state changes
    if (m_state == RunPlugState::FINISHED)
        return m_state;

    if (prev == m_state) {
        if (timeout > 0) {
            m_cond.WaitWithTimeout(&m_lock, absl::Seconds(timeout));
        } else {
            m_cond.Wait(&m_lock);
        }
    }

    // after Wait we hold the lock
    auto state = m_state;
    m_lock.Unlock();

    return state;
}
