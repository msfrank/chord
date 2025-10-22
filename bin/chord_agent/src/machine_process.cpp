
#include <chord_agent/machine_process.h>
#include <chord_agent/machine_supervisor.h>
#include <tempo_utils/log_stream.h>

chord_agent::MachineProcess::MachineProcess(
    const tempo_utils::Url &machineUrl,
    const tempo_utils::ProcessInvoker &invoker,
    MachineSupervisor *supervisor)
    : m_machineUrl(machineUrl),
      m_invoker(invoker),
      m_supervisor(supervisor),
      m_state(MachineState::Initial),
      m_exitStatus(-1)
{
    TU_ASSERT (m_machineUrl.isValid());
    TU_ASSERT (m_invoker.isValid());
    TU_ASSERT (m_supervisor != nullptr);

    m_lock = new absl::Mutex();
    memset(&m_process, 0, sizeof(uv_process_t));
    m_process.data = this;
    m_logger = std::make_unique<MachineLogger>(m_machineUrl, m_supervisor->getLoop());
}

chord_agent::MachineProcess::~MachineProcess()
{
    delete m_lock;
}

/**
 * Returns the url associated with the machine.
 *
 * @return The machine url.
 */
tempo_utils::Url
chord_agent::MachineProcess::getMachineUrl() const
{
    return m_machineUrl;
}

/**
 * Returns the current state of the machine.
 *
 * @return The current state of the machine.
 */
chord_agent::MachineState
chord_agent::MachineProcess::getState() const
{
    absl::MutexLock locker(m_lock);
    return m_state;
}

/**
 * Sets the current state of the machine.
 *
 * @param state The current state of the machine.
 */
void
chord_agent::MachineProcess::setState(MachineState state)
{
    absl::MutexLock locker(m_lock);
    m_state = state;
}

/**
 * Async callback which is called when the machine process exits.
 *
 * @param child The UV process handle.
 * @param status The exit status of the process.
 * @param signal The signal which caused the process to exit, if applicable.
 */
void
chord_agent::on_process_exit(uv_process_t *child, int64_t status, int signal)
{
    auto *machine = (MachineProcess *) child->data;
    TU_LOG_INFO << "child process " << child->pid << " exited with status " << status;
    machine->release(status, signal);
}

/**
 * Spawn the machine process using the specified cwd as the working directory and update
 * the state to Created.
 *
 * @param cwd The working directory.
 * @return Ok status if the process was spawned successfully, otherwise notOk status.
 */
tempo_utils::Status
chord_agent::MachineProcess::spawn(const std::filesystem::path &cwd)
{
    absl::MutexLock locker(m_lock);

    if (m_state != MachineState::Initial)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation,
            "cannot spawn process for machine {}: invalid machine state", m_machineUrl.toString());

    //
    TU_RETURN_IF_NOT_OK (m_logger->initialize());

    auto *loop = m_supervisor->getLoop();

    // set process options for spawning the interpreter helper
    uv_process_options_t processOptions;
    memset(&processOptions, 0, sizeof(uv_process_options_t));
    processOptions.file = m_invoker.getExecutable();
    processOptions.args = m_invoker.getArgv();
    processOptions.cwd = cwd.c_str();
    processOptions.exit_cb = on_process_exit;
    TU_LOG_V << "process invocation: " << m_invoker;
    TU_LOG_V << "process cwd: " << processOptions.cwd;

    // configure child process IO
    uv_stdio_container_t processStdio[3];
    processStdio[0].flags = UV_IGNORE;
    processStdio[1].flags = (uv_stdio_flags) (UV_CREATE_PIPE | UV_WRITABLE_PIPE);
    processStdio[1].data.stream = m_logger->getOutput();
    processStdio[2].flags = (uv_stdio_flags) (UV_CREATE_PIPE | UV_WRITABLE_PIPE);
    processStdio[2].data.stream = m_logger->getError();
    processOptions.stdio = processStdio;
    processOptions.stdio_count = 3;

    // supervisor forks a child process and executes the interpreter helper
    auto ret = uv_spawn(loop, &m_process, &processOptions);
    if (ret < 0)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation,
            "failed to spawn child process: {} ({})", uv_strerror(ret), uv_err_name(ret));

    // update the machine status
    m_state = MachineState::Created;

    TU_LOG_INFO << "spawned machine process " << m_machineUrl << " with pid " << m_process.pid;

    return tempo_utils::Status();
}

/**
 *
 * @param signal
 * @return
 */
tempo_utils::Status
chord_agent::MachineProcess::terminate(int signal)
{
    absl::MutexLock locker(m_lock);

    if (m_state == MachineState::Exited)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation,
            "cannot terminate process for machine {}: invalid machine state", m_machineUrl.toString());

    // if signal number was not specified, then use the default termination signal
    if (signal < 0) {
        signal = SIGTERM;
    }

    // send the termination signal
    auto ret = uv_kill(m_process.pid, signal);
    if (ret < 0)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation,
            "failed to terminate child process: {} ({})", uv_strerror(ret), uv_err_name(ret));

    // update the machine status
    m_state = MachineState::Terminating;

    return tempo_utils::Status();
}

/**
 * Release the machine process from the supervisor and update the state to Exited.
 *
 * @param status The exit status.
 * @param signal The termination signal.
 */
void
chord_agent::MachineProcess::release(tu_int64 status, int signal)
{
    // update internal state while holding the lock
    {
        absl::MutexLock locker(m_lock);
        m_state = MachineState::Exited;
        m_exitStatus = status;
        m_exitSignal = signal;
    }

    // signal the supervisor to release after releasing the lock
    auto status_ = m_supervisor->release(m_machineUrl, status, signal);
    TU_LOG_WARN_IF(status_.notOk()) << "failed to release machine " << m_machineUrl << ": " << status_;
}