
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include <uv.h>

#include <chord_agent/machine_supervisor.h>
#include <tempo_utils/log_stream.h>

#include "chord_agent/agent_result.h"

chord_agent::MachineSupervisor::MachineSupervisor(
    const AgentConfig &agentConfig,
    const chord_common::TransportLocation &supervisorEndpoint,
    uv_loop_t *loop)
    : m_agentConfig(agentConfig),
      m_supervisorEndpoint(supervisorEndpoint),
      m_loop(loop),
      m_shuttingDown(false)
{
    TU_ASSERT (m_supervisorEndpoint.isValid());
    TU_ASSERT (m_loop != nullptr);
}

chord_agent::MachineSupervisor::~MachineSupervisor()
{
}

static void
on_idle_timer(uv_timer_t *timer)
{
    auto *supervisor = (chord_agent::MachineSupervisor *) timer->data;
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
chord_agent::MachineSupervisor::initialize()
{
    absl::MutexLock locker(&m_lock);

    uv_timer_init(m_loop, &m_idle);
    m_idle.data = this;

    // if a positive idle timeout is defined, then start the idle timer
    auto idleTimeoutMillis = absl::ToInt64Milliseconds(m_agentConfig.idleTimeout);
    if (idleTimeoutMillis > 0) {
        uv_timer_start(&m_idle, on_idle_timer, idleTimeoutMillis, 0);
    }
    return {};
}

/**
 * Check whether the supervisor is idle (there are no machines running or waiting).
 *
 * @return true if the supervisor is idle, otherwise false.
 */
bool
chord_agent::MachineSupervisor::isIdle()
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
chord_agent::MachineSupervisor::getLoop() const
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
chord_agent::on_spawning_timeout(uv_timer_t *timer)
{
    auto *ctx = (SpawningContext *) timer->data;
    ctx->supervisor->abandon(ctx->machineName);
}

/**
 * Async callback which is called when we time out waiting for a RunMachine request from
 * the client.
 *
 * @param timer The signing timer.
 */
void
chord_agent::on_signing_timeout(uv_timer_t *timer)
{
    auto *ctx = (SigningContext *) timer->data;
    ctx->supervisor->abandon(ctx->machineName);
}

/**
 * Async callback which is called when we time out waiting for a AdvertiseEndpoints request
 * from the machine process.
 *
 * @param timer The ready timer.
 */
void
chord_agent::on_ready_timeout(uv_timer_t *timer)
{
    auto *ctx = (ReadyContext *) timer->data;
    ctx->supervisor->abandon(ctx->machineName);
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
chord_agent::MachineSupervisor::spawnMachine(
    std::string_view machineName,
    const zuri_packager::PackageSpecifier &mainPackage,
    const MachineOptions &options,
    std::shared_ptr<OnSupervisorSpawn> waiter)
{
    TU_LOG_INFO << "spawnMachine " << machineName;

    absl::MutexLock locker(&m_lock);

    // block spawning if supervisor is shutting down
    if (m_shuttingDown)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "supervisor is shutting down");

    // verify there is not an existing machine with the machine url
    if (m_machines.contains(machineName))
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "machine '{}' already exists", machineName);

    // create the machine process
    std::shared_ptr<MachineProcess> machine;
    TU_ASSIGN_OR_RETURN (machine, MachineProcess::create(
        machineName, mainPackage, m_supervisorEndpoint, this, options));

    // spawn the machine process
    TU_RETURN_IF_NOT_OK (machine->spawn());

    int ret;

    // unconditionally stop the idle timer
    ret = uv_timer_stop(&m_idle);
    if (ret < 0)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "failed to stop the idle timer: {}", uv_strerror(ret));

    // create the spawning context
    auto spawning = std::make_unique<SpawningContext>();
    spawning->machineName = machineName;
    spawning->supervisor = this;
    spawning->waiter = waiter;

    // initialize the timer
    ret = uv_timer_init(m_loop, &spawning->timeout);
    if (ret < 0)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "failed to initialize the spawning timer: {}", uv_strerror(ret));
    spawning->timeout.data = spawning.get();

    // start the timer
    auto registrationTimeoutMillis = absl::ToInt64Milliseconds(m_agentConfig.registrationTimeout);
    ret = uv_timer_start(&spawning->timeout, on_spawning_timeout, registrationTimeoutMillis, 0);
    if (ret < 0)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "failed to start the spawning timer: {}", uv_strerror(ret));

    // change the machine state to Starting
    machine->setState(MachineState::Starting);

    // add the machine process to the machines map
    m_machines[machineName] = std::move(machine);

    // track the spawning ctx
    m_spawning[machineName] = std::move(spawning);

    TU_LOG_V << "spawning machine " << machineName;

    return {};
}

/**
 *
 * @param machineUrl
 * @param signCertificatesRequest
 * @param waiter
 * @return Ok status if the operation completed successfully, otherwise notOk status.
 */
tempo_utils::Status
chord_agent::MachineSupervisor::requestCertificates(
    std::string_view machineName,
    const chord_invoke::SignCertificatesRequest &signCertificatesRequest,
    std::shared_ptr<OnSupervisorSign> waiter)
{
    TU_LOG_INFO << "requestCertificates " << machineName;

    absl::MutexLock locker(&m_lock);

    if (!m_spawning.contains(machineName))
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "machine '{}' is not spawning", machineName);

    // remove the spawning context for the specified machine
    auto node = m_spawning.extract(machineName);
    auto spawning = std::move(node.mapped());

    int ret;

    // cancel the spawn timeout
    ret = uv_timer_stop(&spawning->timeout);
    if (ret < 0)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "failed to stop spawning timer: {}", uv_strerror(ret));
    uv_close((uv_handle_t *) &spawning->timeout, nullptr);

    // complete the CreateMachine call, which passes the CSRs from SignCertificates back to the client
    MachineHandle handle;
    handle.machineName = spawning->machineName;
    spawning->waiter->onComplete(handle, signCertificatesRequest);
    spawning.reset();

    // create a new signing context
    auto signing = std::make_unique<SigningContext>();
    signing->machineName = machineName;
    signing->supervisor = this;
    signing->waiter = waiter;

    // initialize the timer
    ret = uv_timer_init(m_loop, &signing->timeout);
    if (ret < 0)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "failed to initialize the signing timer: {}", uv_strerror(ret));
    signing->timeout.data = signing.get();

    // start the timer
    auto registrationTimeoutMillis = absl::ToInt64Milliseconds(m_agentConfig.registrationTimeout);
    ret = uv_timer_start(&signing->timeout, on_signing_timeout, registrationTimeoutMillis, 0);
    if (ret < 0)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "failed to start the signing timer: {}", uv_strerror(ret));

    m_signing[machineName] = std::move(signing);

    TU_LOG_V << "signing certificates for machine " << machineName;

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
chord_agent::MachineSupervisor::bindCertificates(
    std::string_view machineName,
    const chord_invoke::RunMachineRequest &runMachineRequest,
    std::shared_ptr<OnSupervisorReady> waiter)
{
    TU_LOG_INFO << "bindCertificates " << machineName;

    absl::MutexLock locker(&m_lock);

    if (!m_signing.contains(machineName))
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "machine '{}' is not signing", machineName);

    // remove the spawning context for the specified machine
    auto node = m_signing.extract(machineName);
    auto signing = std::move(node.mapped());

    int ret;

    // cancel the spawn timeout
    ret = uv_timer_stop(&signing->timeout);
    if (ret < 0)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "failed to stop signing timer: {}", uv_strerror(ret));
    uv_close((uv_handle_t *) &signing->timeout, nullptr);

    // complete the SignCertificates call, which passes the certs from RunMachine back to the machine
    MachineHandle handle;
    handle.machineName = signing->machineName;
    signing->waiter->onComplete(handle, runMachineRequest);
    signing.reset();

    // create a new ready context
    auto ready = std::make_unique<ReadyContext>();
    ready->machineName = machineName;
    ready->supervisor = this;
    ready->waiter = waiter;

    // initialize the timer
    ret = uv_timer_init(m_loop, &ready->timeout);
    if (ret < 0)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "failed to initialize the ready timer: {}", uv_strerror(ret));
    ready->timeout.data = ready.get();

    // start the timer
    auto registrationTimeoutMillis = absl::ToInt64Milliseconds(m_agentConfig.registrationTimeout);
    ret = uv_timer_start(&ready->timeout, on_ready_timeout, registrationTimeoutMillis, 0);
    if (ret < 0)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "failed to start the ready timer: {}", uv_strerror(ret));

    m_ready[machineName] = std::move(ready);

    TU_LOG_V << "binding certificates for machine " << machineName;

    return {};
}

/**
 *
 * @param machineUrl
 * @param advertiseEndpointsRequest
 * @return Ok status if the operation completed successfully, otherwise notOk status.
 */
tempo_utils::Status
chord_agent::MachineSupervisor::startMachine(
    std::string_view machineName,
    const chord_invoke::AdvertiseEndpointsRequest &advertiseEndpointsRequest)
{
    TU_LOG_INFO << "startMachine " << machineName;

    absl::MutexLock locker(&m_lock);

    if (!m_ready.contains(machineName))
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "machine '{}' is not ready", machineName);

    // remove the ready context for the specified machine
    auto node = m_ready.extract(machineName);
    auto ready = std::move(node.mapped());

    int ret;

    // cancel the spawn timeout
    ret = uv_timer_stop(&ready->timeout);
    if (ret < 0)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "failed to stop ready timer: {}", uv_strerror(ret));
    uv_close((uv_handle_t *) &ready->timeout, nullptr);

    // change the machine state to Pending
    auto &machine = m_machines.at(machineName);
    machine->setState(MachineState::Running);

    // complete the RunMachine call, which passes the bound endpoints from AdvertiseEndpoints to the client
    MachineHandle handle;
    handle.machineName = ready->machineName;
    ready->waiter->onComplete(handle, advertiseEndpointsRequest);
    ready.reset();

    TU_LOG_V << "starting machine " << machineName;

    return {};
}

/**
 * Terminate the process of the specified machine.
 *
 * @param machineUrl The machine url.
 * @param waiter The waiter which will be completed once the process has terminated.
 * @return Ok status if the operation completed successfully, otherwise notOk status.
 */
tempo_utils::Status
chord_agent::MachineSupervisor::terminateMachine(
    std::string_view machineName,
    std::shared_ptr<OnSupervisorTerminate> waiter)
{
    TU_LOG_INFO << "terminateMachine " << machineName;

    absl::MutexLock locker(&m_lock);

    if (!m_machines.contains(machineName))
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "machine '{}' does not exist", machineName);

    auto &machine = m_machines.at(machineName);
    switch (machine->getState()) {
        case MachineState::Created:
        case MachineState::Starting:
        case MachineState::Running:
        case MachineState::Terminating:
            break;
        default:
            return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
                "invalid machine state");
    }

    // we can only have a single termination waiter
    if (m_waiting.contains(machineName))
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "machine '{}' is already terminating", machineName);

    // create the waiting context
    auto waiting = std::make_unique<WaitingContext>();
    waiting->waiter = waiter;
    m_waiting[machineName] = std::move(waiting);

    TU_LOG_V << "terminating machine " << machineName;

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
chord_agent::MachineSupervisor::abandon(std::string_view machineName)
{
    TU_LOG_INFO << "abandon " << machineName;

    absl::MutexLock locker(&m_lock);

    // verify the machine exists
    if (!m_machines.contains(machineName))
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "invalid machine state");

    // verify the machine is in one of the initializing states
    auto &machine = m_machines.at(machineName);
    switch (machine->getState()) {
        case MachineState::Created:
        case MachineState::Starting:
            break;
        default:
            return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
                "invalid machine state");
    }

    // we can only have a single termination waiter
    if (m_waiting.contains(machineName))
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "machine '{}' is already terminating", machineName);

    // remove the machine from the appropriate queue and fail the associated waiter
    if (m_spawning.contains(machineName)) {
        auto node = m_spawning.extract(machineName);
        auto &spawning = node.mapped();
        uv_timer_stop(&spawning->timeout);
        uv_close((uv_handle_t *) &spawning->timeout, nullptr);
        spawning->waiter->onStatus(AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "abandoned machine"));
    } else if (m_signing.contains(machineName)) {
        auto node = m_signing.extract(machineName);
        auto &signing = node.mapped();
        uv_timer_stop(&signing->timeout);
        uv_close((uv_handle_t *) &signing->timeout, nullptr);
        signing->waiter->onStatus(AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "abandoned machine"));
    } else if (m_ready.contains(machineName)) {
        auto node = m_ready.extract(machineName);
        auto &ready = node.mapped();
        uv_timer_stop(&ready->timeout);
        uv_close((uv_handle_t *) &ready->timeout, nullptr);
        ready->waiter->onStatus(AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "abandoned machine"));
    } else {
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "invalid machine state");
    }

    // create the waiting context
    auto waiting = std::make_unique<WaitingContext>();
    waiting->waiter = std::make_shared<OnInternalTerminate>();
    m_waiting[machineName] = std::move(waiting);

    TU_LOG_V << "abandoning machine " << machineName;

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
chord_agent::MachineSupervisor::release(std::string_view machineName, tu_int64 status, int signal)
{
    TU_LOG_INFO << "release " << machineName;

    absl::MutexLock locker(&m_lock);

    if (!m_machines.contains(machineName))
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "machine '{}' does not exist", machineName);

    ExitStatus exitStatus;
    exitStatus.machineName = machineName;
    exitStatus.status = status;
    exitStatus.signal = signal;

    // if there is a waiter then extract it and invoke the callback
    if (m_waiting.contains(machineName)) {
        auto node = m_waiting.extract(machineName);
        auto &waiting = node.mapped();
        waiting->waiter->onComplete(exitStatus);
    }

    // remove the machine
    m_machines.extract(machineName);

    return {};
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
chord_agent::MachineSupervisor::shutdown()
{
    absl::MutexLock locker(&m_lock);

    TU_ASSERT (!m_shuttingDown);
    m_shuttingDown = true;
    uv_close((uv_handle_t *) &m_idle, nullptr);
    return {};
}

chord_agent::OnInternalTerminate::OnInternalTerminate()
{
}

void
chord_agent::OnInternalTerminate::onComplete(ExitStatus exitStatus)
{
    TU_LOG_V << "internal termination completed";
}

void
chord_agent::OnInternalTerminate::onStatus(tempo_utils::Status status)
{
    TU_LOG_V << "internal termination failed: " << status;
}
