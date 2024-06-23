
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <uv.h>

#include <chord_agent/machine_supervisor.h>
#include <tempo_utils/log_stream.h>

MachineSupervisor::MachineSupervisor(
    uv_loop_t *loop,
    const std::filesystem::path &processRunDirectory,
    int idleTimeoutSeconds,
    int registrationTimeoutSeconds)
    : m_loop(loop),
      m_processRunDirectory(processRunDirectory),
      m_idleTimeoutSeconds(idleTimeoutSeconds),
      m_registrationTimeoutSeconds(registrationTimeoutSeconds),
      m_shuttingDown(false)
{
    TU_ASSERT (m_loop != nullptr);
}

MachineSupervisor::~MachineSupervisor()
{
}

static void
on_idle_timer(uv_timer_t *timer)
{
    auto *supervisor = (MachineSupervisor *) timer->data;
    if (supervisor->isIdle()) {
        TU_LOG_INFO << "idle timeout exceeded";
        uv_stop(timer->loop);
    }
}

/**
 * Initialize the machine supervisor.
 *
 * @return Ok status if supervisor initialization completed successfully, otherwise notOk status.
 */
tempo_utils::Status
MachineSupervisor::initialize()
{
    absl::MutexLock locker(&m_lock);

    uv_timer_init(m_loop, &m_idle);
    m_idle.data = this;

    // if a positive idle timeout is defined, then start the idle timer
    if (m_idleTimeoutSeconds > 0) {
        uv_timer_start(&m_idle, on_idle_timer, m_idleTimeoutSeconds * 1000, 0);
    }
    return tempo_utils::GenericStatus::ok();
}

/**
 * Check whether the supervisor is idle (there are no machines running or waiting).
 *
 * @return true if the supervisor is idle, otherwise false.
 */
bool
MachineSupervisor::isIdle()
{
    absl::MutexLock locker(&m_lock);
    return m_machines.empty() && m_waiting.empty();
}

/**
 * Get the UV main loop.
 *
 * @return The main loop.
 */
uv_loop_t *
MachineSupervisor::getLoop() const
{
    return m_loop;
}

/**
 * Async callback which is called when we time out waiting for a SignCertificates request
 * from the machine process.
 *
 * @param timer The spawning timer.
 */
void
on_spawning_timeout(uv_timer_t *timer)
{
    auto *ctx = (SpawningContext *) timer->data;
    ctx->supervisor->abandon(ctx->url);
}

/**
 * Async callback which is called when we time out waiting for a RunMachine request from
 * the client.
 *
 * @param timer The signing timer.
 */
void
on_signing_timeout(uv_timer_t *timer)
{
    auto *ctx = (SigningContext *) timer->data;
    ctx->supervisor->abandon(ctx->url);
}

/**
 * Async callback which is called when we time out waiting for a AdvertiseEndpoints request
 * from the machine process.
 *
 * @param timer The ready timer.
 */
void
on_ready_timeout(uv_timer_t *timer)
{
    auto *ctx = (ReadyContext *) timer->data;
    ctx->supervisor->abandon(ctx->url);
}

/**
 * Create a machine by spawning a new subprocess.
 *
 * @param machineUrl
 * @param process
 * @param waiter
 * @return Ok status if machine process spawned successfully, otherwise notOk status.
 */
tempo_utils::Status
MachineSupervisor::spawnMachine(
    const tempo_utils::Url &machineUrl,
    const tempo_utils::ProcessInvoker &invoker,
    std::shared_ptr<OnSupervisorSpawn> waiter)
{
    TU_LOG_INFO << "spawnMachine " << machineUrl;

    absl::MutexLock locker(&m_lock);

    // block spawning if supervisor is shutting down
    if (m_shuttingDown)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "supervisor is shutting down");

    // verify there is not an existing machine with the machine url
    if (m_machines.contains(machineUrl))
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "machine already exists");

    // create the machine process
    auto machine = std::make_unique<MachineProcess>(machineUrl, invoker, this);

    // spawn the machine process
    TU_RETURN_IF_NOT_OK (machine->spawn(m_processRunDirectory));

    int ret;

    // unconditionally stop the idle timer
    ret = uv_timer_stop(&m_idle);
    TU_LOG_FATAL_IF(ret < 0) << "failed to stop the idle timer";

    // create the spawning context
    auto spawning = std::make_unique<SpawningContext>();
    spawning->url = machineUrl;
    spawning->supervisor = this;
    spawning->waiter = waiter;

    // initialize the timer
    ret = uv_timer_init(m_loop, &spawning->timeout);
    TU_LOG_FATAL_IF(ret < 0) << "failed to initialize the spawning timer";
    spawning->timeout.data = spawning.get();

    // start the timer
    ret = uv_timer_start(&spawning->timeout, on_spawning_timeout, m_registrationTimeoutSeconds * 1000, 0);
    TU_LOG_FATAL_IF(ret < 0) << "failed to start the spawning timer";

    // change the machine state to Starting
    machine->setState(MachineState::Starting);

    // add the machine process to the machines map
    m_machines[machineUrl] = std::move(machine);

    // track the spawning ctx
    m_spawning[machineUrl] = std::move(spawning);

    TU_LOG_V << "spawning machine " << machineUrl;

    return tempo_utils::GenericStatus::ok();
}

/**
 *
 * @param machineUrl
 * @param signCertificatesRequest
 * @param waiter
 * @return Ok status if the operation completed successfully, otherwise notOk status.
 */
tempo_utils::Status
MachineSupervisor::requestCertificates(
    const tempo_utils::Url &machineUrl,
    const chord_invoke::SignCertificatesRequest &signCertificatesRequest,
    std::shared_ptr<OnSupervisorSign> waiter)
{
    TU_LOG_INFO << "requestCertificates " << machineUrl;

    absl::MutexLock locker(&m_lock);

    if (!m_spawning.contains(machineUrl))
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "machine is not spawning");

    // remove the spawning context for the specified machine
    auto node = m_spawning.extract(machineUrl);
    auto spawning = std::move(node.mapped());

    int ret;

    // cancel the spawn timeout
    ret = uv_timer_stop(&spawning->timeout);
    TU_LOG_FATAL_IF(ret < 0) << "failed to stop spawning timer";
    uv_close((uv_handle_t *) &spawning->timeout, nullptr);

    // complete the CreateMachine call, which passes the CSRs from SignCertificates back to the client
    MachineHandle handle;
    handle.url = spawning->url;
    spawning->waiter->onComplete(handle, signCertificatesRequest);
    spawning.reset();

    // create a new signing context
    auto signing = std::make_unique<SigningContext>();
    signing->url = machineUrl;
    signing->supervisor = this;
    signing->waiter = waiter;

    // initialize the timer
    ret = uv_timer_init(m_loop, &signing->timeout);
    TU_LOG_FATAL_IF(ret < 0) << "failed to initialize the signing timer";
    signing->timeout.data = signing.get();

    // start the timer
    ret = uv_timer_start(&signing->timeout, on_signing_timeout, m_registrationTimeoutSeconds * 1000, 0);
    TU_LOG_FATAL_IF(ret < 0) << "failed to start the signing timer";

    m_signing[machineUrl] = std::move(signing);

    TU_LOG_V << "signing certificates for machine " << machineUrl;

    return tempo_utils::GenericStatus::ok();
}

/**
 *
 * @param machineUrl
 * @param runMachineRequest
 * @param waiter
 * @return Ok status if the operation completed successfully, otherwise notOk status.
 */
tempo_utils::Status
MachineSupervisor::bindCertificates(
    const tempo_utils::Url &machineUrl,
    const chord_invoke::RunMachineRequest &runMachineRequest,
    std::shared_ptr<OnSupervisorReady> waiter)
{
    TU_LOG_INFO << "bindCertificates " << machineUrl;

    absl::MutexLock locker(&m_lock);

    if (!m_signing.contains(machineUrl))
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "machine is not signing");

    // remove the spawning context for the specified machine
    auto node = m_signing.extract(machineUrl);
    auto signing = std::move(node.mapped());

    int ret;

    // cancel the spawn timeout
    ret = uv_timer_stop(&signing->timeout);
    TU_LOG_FATAL_IF(ret < 0) << "failed to stop signing timer";
    uv_close((uv_handle_t *) &signing->timeout, nullptr);

    // complete the SignCertificates call, which passes the certs from RunMachine back to the machine
    MachineHandle handle;
    handle.url = signing->url;
    signing->waiter->onComplete(handle, runMachineRequest);
    signing.reset();

    // create a new ready context
    auto ready = std::make_unique<ReadyContext>();
    ready->url = machineUrl;
    ready->supervisor = this;
    ready->waiter = waiter;

    // initialize the timer
    ret = uv_timer_init(m_loop, &ready->timeout);
    TU_LOG_FATAL_IF(ret < 0) << "failed to initialize the ready timer";
    ready->timeout.data = ready.get();

    // start the timer
    ret = uv_timer_start(&ready->timeout, on_ready_timeout, m_registrationTimeoutSeconds * 1000, 0);
    TU_LOG_FATAL_IF(ret < 0) << "failed to start the ready timer";

    m_ready[machineUrl] = std::move(ready);

    TU_LOG_V << "binding certificates for machine " << machineUrl;

    return tempo_utils::GenericStatus::ok();
}

/**
 *
 * @param machineUrl
 * @param advertiseEndpointsRequest
 * @return Ok status if the operation completed successfully, otherwise notOk status.
 */
tempo_utils::Status
MachineSupervisor::startMachine(
    const tempo_utils::Url &machineUrl,
    const chord_invoke::AdvertiseEndpointsRequest &advertiseEndpointsRequest)
{
    TU_LOG_INFO << "startMachine " << machineUrl;

    absl::MutexLock locker(&m_lock);

    if (!m_ready.contains(machineUrl))
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "machine is not ready");

    // remove the ready context for the specified machine
    auto node = m_ready.extract(machineUrl);
    auto ready = std::move(node.mapped());

    int ret;

    // cancel the spawn timeout
    ret = uv_timer_stop(&ready->timeout);
    TU_LOG_FATAL_IF(ret < 0) << "failed to stop ready timer";
    uv_close((uv_handle_t *) &ready->timeout, nullptr);

    // change the machine state to Pending
    auto &machine = m_machines.at(machineUrl);
    machine->setState(MachineState::Running);

    // complete the RunMachine call, which passes the bound endpoints from AdvertiseEndpoints to the client
    MachineHandle handle;
    handle.url = ready->url;
    ready->waiter->onComplete(handle, advertiseEndpointsRequest);
    ready.reset();

    TU_LOG_V << "starting machine " << machineUrl;

    return tempo_utils::GenericStatus::ok();
}

/**
 * Terminate the process of the specified machine.
 *
 * @param machineUrl The machine url.
 * @param waiter The waiter which will be completed once the process has terminated.
 * @return Ok status if the operation completed successfully, otherwise notOk status.
 */
tempo_utils::Status
MachineSupervisor::terminateMachine(
    const tempo_utils::Url &machineUrl,
    std::shared_ptr<OnSupervisorTerminate> waiter)
{
    TU_LOG_INFO << "terminateMachine " << machineUrl;

    absl::MutexLock locker(&m_lock);

    if (!m_machines.contains(machineUrl))
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "machine does not exist");

    auto &machine = m_machines.at(machineUrl);
    switch (machine->getState()) {
        case MachineState::Created:
        case MachineState::Starting:
        case MachineState::Running:
        case MachineState::Terminating:
            break;
        default:
            return tempo_utils::GenericStatus::forCondition(
                tempo_utils::GenericCondition::kInternalViolation, "invalid machine state");
    }

    // we can only have a single termination waiter
    if (m_waiting.contains(machineUrl))
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "machine is already terminating");

    // create the waiting context
    auto waiting = std::make_unique<WaitingContext>();
    waiting->waiter = waiter;
    m_waiting[machineUrl] = std::move(waiting);

    TU_LOG_V << "terminating maching " << machineUrl;

    // terminate the machine
    return machine->terminate(SIGTERM);
}

/**
 * Abandons the machine process which is not completely initialized. This method is intended
 * only to be called internally from the timeout callbacks.
 *
 * @param machineUrl The machine url.
 * @return Ok status if the operation completed successfully, otherwise notOk status.
 */
tempo_utils::Status
MachineSupervisor::abandon(const tempo_utils::Url &machineUrl)
{
    TU_LOG_INFO << "abandon " << machineUrl;

    absl::MutexLock locker(&m_lock);

    // verify the machine exists
    if (!m_machines.contains(machineUrl))
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "invalid machine state");

    // verify the machine is in one of the initializing states
    auto &machine = m_machines.at(machineUrl);
    switch (machine->getState()) {
        case MachineState::Created:
        case MachineState::Starting:
            break;
        default:
            return tempo_utils::GenericStatus::forCondition(
                tempo_utils::GenericCondition::kInternalViolation, "invalid machine state");
    }

    // we can only have a single termination waiter
    if (m_waiting.contains(machineUrl))
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "machine is already terminating");

    // remove the machine from the appropriate queue and fail the associated waiter
    if (m_spawning.contains(machineUrl)) {
        auto node = m_spawning.extract(machineUrl);
        auto &spawning = node.mapped();
        uv_timer_stop(&spawning->timeout);
        uv_close((uv_handle_t *) &spawning->timeout, nullptr);
        spawning->waiter->onStatus(tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "abandoned machine"));
    } else if (m_signing.contains(machineUrl)) {
        auto node = m_signing.extract(machineUrl);
        auto &signing = node.mapped();
        uv_timer_stop(&signing->timeout);
        uv_close((uv_handle_t *) &signing->timeout, nullptr);
        signing->waiter->onStatus(tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "abandoned machine"));
    } else if (m_ready.contains(machineUrl)) {
        auto node = m_ready.extract(machineUrl);
        auto &ready = node.mapped();
        uv_timer_stop(&ready->timeout);
        uv_close((uv_handle_t *) &ready->timeout, nullptr);
        ready->waiter->onStatus(tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "abandoned machine"));
    } else {
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "invalid machine state");
    }

    // create the waiting context
    auto waiting = std::make_unique<WaitingContext>();
    waiting->waiter = std::make_shared<OnInternalTerminate>();
    m_waiting[machineUrl] = std::move(waiting);

    TU_LOG_V << "abandoning machine " << machineUrl;

    // terminate the machine
    return machine->terminate(SIGTERM);
}

/**
 * Releases a machine which has exited. This method is intended only to be called internally in
 * response to the process termination callback. This method will signal a waiter if the process
 * was terminated explicitly via terminateMachine or abandon, but in the case of the process
 * self-terminating there will be no associated waiter.
 *
 * @param machineUrl The machine url.
 * @param status The exit status.
 * @param signal The termination signal.
 * @return Ok status if the operation completed successfully, otherwise notOk status.
 */
tempo_utils::Status
MachineSupervisor::release(const tempo_utils::Url &machineUrl, tu_int64 status, int signal)
{
    TU_LOG_INFO << "release " << machineUrl;

    absl::MutexLock locker(&m_lock);

    if (!m_machines.contains(machineUrl))
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "machine does not exist");

    ExitStatus exitStatus;
    exitStatus.url = machineUrl;
    exitStatus.status = status;
    exitStatus.signal = signal;

    // if there is a waiter then extract it and invoke the callback
    if (m_waiting.contains(machineUrl)) {
        auto node = m_waiting.extract(machineUrl);
        auto &waiting = node.mapped();
        waiting->waiter->onComplete(exitStatus);
    }

    // remove the machine
    m_machines.extract(machineUrl);

    return tempo_utils::GenericStatus::ok();
}

//tempo_utils::Status
//MachineSupervisor::reap(const tempo_utils::Url &machineUrl)
//{
//    absl::MutexLock locker(&m_lock);
//
//    if (m_running.contains(machineUrl))
//        return tempo_utils::GenericStatus::forCondition(
//            tempo_utils::GenericCondition::kInternalViolation, "machine is still running");
//    if (!m_waiting.contains(machineUrl))
//        return tempo_utils::GenericStatus::forCondition(
//            tempo_utils::GenericCondition::kInternalViolation, "machine is not waiting");
//
//    auto node = m_waiting.extract(machineUrl);
//    auto *waiting = node.mapped();
//    delete waiting;
//
//    // if idle timeout is nonzero and supervisor is idle then restart the idle timer
//    if (m_idleTimeoutSeconds > 0 && m_running.empty() && m_waiting.empty()) {
//        uv_timer_start(&m_idle, on_idle_timer, m_idleTimeoutSeconds * 1000, 0);
//    }
//
//    return tempo_utils::GenericStatus::ok();
//}

/**
 *
 * @return Ok status if supervisor shutdown completed successfully, otherwise notOk status.
 */
tempo_utils::Status
MachineSupervisor::shutdown()
{
    absl::MutexLock locker(&m_lock);

    TU_ASSERT (!m_shuttingDown);
    m_shuttingDown = true;
    uv_close((uv_handle_t *) &m_idle, nullptr);
    return tempo_utils::GenericStatus::ok();
}

OnInternalTerminate::OnInternalTerminate()
{
}

void
OnInternalTerminate::onComplete(ExitStatus exitStatus)
{
    TU_LOG_V << "internal termination completed";
}

void
OnInternalTerminate::onStatus(tempo_utils::Status status)
{
    TU_LOG_V << "internal termination failed: " << status;
}
