
#include <uv.h>

#include <chord_agent/agent_result.h>
#include <chord_agent/agent_service.h>
#include <chord_common/grpc_status.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/container_conversions.h>
#include <tempo_config/parse_config.h>
#include <tempo_utils/file_utilities.h>

#include "chord_common/common_types.h"

chord_agent::AgentService::AgentService(const AgentConfig &agentConfig, uv_loop_t *loop)
    : m_agentConfig(agentConfig),
      m_loop(loop)
{
    TU_ASSERT (m_loop != nullptr);
    uv_timeval64_t tv;
    uv_gettimeofday(&tv);
    m_uptime = tv.tv_sec;
    m_lock = std::make_unique<absl::Mutex>();
}

tempo_utils::Status
chord_agent::AgentService::initialize(const chord_common::TransportLocation &supervisorEndpoint)
{
    absl::MutexLock lock(m_lock.get());
    if (m_supervisor != nullptr)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "AgentService is already initialized");
    m_supervisor = std::make_unique<MachineSupervisor>(m_agentConfig, supervisorEndpoint, m_loop);
    return {};
}

tempo_utils::Status
chord_agent::AgentService::shutdown()
{
    absl::MutexLock lock(m_lock.get());
    if (m_supervisor == nullptr)
        return {};
    return m_supervisor->shutdown();
}

grpc::ServerUnaryReactor *
chord_agent::AgentService::IdentifyAgent(
    grpc::CallbackServerContext *context,
    const chord_invoke::IdentifyAgentRequest *request,
    chord_invoke::IdentifyAgentResult *response)
{
    uv_timeval64_t tv;
    uv_gettimeofday(&tv);
    response->set_agent_name(m_agentConfig.sessionName);
    response->set_uptime_millis(tv.tv_sec - m_uptime);
    auto *reactor = context->DefaultReactor();
    reactor->Finish(grpc::Status::OK);
    return reactor;
}

tempo_utils::Status
chord_agent::AgentService::doCreateMachine(
    grpc::ServerUnaryReactor *reactor,
    grpc::CallbackServerContext *context,
    const chord_invoke::CreateMachineRequest *request,
    chord_invoke::CreateMachineResult *response)
{
    auto &machineName = request->machine_name();
    auto mainPackage = zuri_packager::PackageSpecifier::fromString(request->main_package());
    MachineOptions options;

    // build list of requested ports
    for (const auto &requestedPort : request->requested_ports()) {
        auto protocolUrl = tempo_utils::Url::fromString(requestedPort.protocol_url());
        if (!protocolUrl.isValid())
            return AgentStatus::forCondition(AgentCondition::kInvalidConfiguration,
                "invalid protocol url '{}'", requestedPort.protocol_url());

        chord_common::PortType portType;
        switch (requestedPort.port_type()) {
            case chord_invoke::Streaming:
                portType = chord_common::PortType::Streaming;
                break;
            case chord_invoke::OneShot:
                portType = chord_common::PortType::OneShot;
                break;
            default:
                return AgentStatus::forCondition(AgentCondition::kInvalidConfiguration,
                    "invalid port type for protocol '{}'", protocolUrl.toString());
        }

        chord_common::PortDirection portDirection;
        switch (requestedPort.port_direction()) {
            case chord_invoke::Client:
                portDirection = chord_common::PortDirection::Client;
                break;
            case chord_invoke::Server:
                portDirection = chord_common::PortDirection::Server;
                break;
            case chord_invoke::BiDirectional:
                portDirection = chord_common::PortDirection::BiDirectional;
                break;
            default:
                return AgentStatus::forCondition(AgentCondition::kInvalidConfiguration,
                    "invalid port direction for protocol '{}'", protocolUrl.toString());
        }

        options.requestedPorts.emplace_back(protocolUrl, portType, portDirection);
    }

    options.enableMonitoring = request->requested_control();
    options.startSuspended = request->start_suspended();

    // acquire mutex and ensure that the service is initialized
    absl::MutexLock lock(m_lock.get());
    if (m_supervisor == nullptr)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "service is not initialized");

    // spawn the helper process and move machine to 'spawning' queue
    auto waiter = std::make_shared<OnAgentSpawn>(reactor, response);
    auto spawnMachineStatus = m_supervisor->spawnMachine(machineName, mainPackage, options, waiter);
    if (spawnMachineStatus.notOk()) {
        waiter->onStatus(spawnMachineStatus);
    }

    return {};
}

grpc::ServerUnaryReactor *
chord_agent::AgentService::CreateMachine(
    grpc::CallbackServerContext *context,
    const chord_invoke::CreateMachineRequest *request,
    chord_invoke::CreateMachineResult *response)
{
    TU_LOG_INFO << "CreateMachine request: " << request->DebugString();
    auto *reactor = context->DefaultReactor();
    auto status = doCreateMachine(reactor, context, request, response);
    if (status.notOk()) {
        reactor->Finish(chord_common::convert_status(status));
    }
    return reactor;
}

tempo_utils::Status
chord_agent::AgentService::doSignCertificates(
    grpc::ServerUnaryReactor *reactor,
    grpc::CallbackServerContext *context,
    const chord_invoke::SignCertificatesRequest *request,
    chord_invoke::SignCertificatesResult *response)
{
    auto &machineName = request->machine_name();

    // verify any duplicate declared ports each have a unique endpoint
    absl::flat_hash_map<tempo_utils::Url, absl::flat_hash_set<tempo_utils::Url>> declaredPorts;
    for (const auto &declaredPort : request->declared_ports()) {

        auto protocolUrl = tempo_utils::Url::fromString(declaredPort.protocol_url());
        if (request->declared_endpoints_size() <= declaredPort.endpoint_index())
            return AgentStatus::forCondition(AgentCondition::kInvalidConfiguration,
                "invalid endpoint index for declared port");

        const auto &declaredEndpoint = request->declared_endpoints(declaredPort.endpoint_index());
        auto endpointUrl = tempo_utils::Url::fromString(declaredEndpoint.endpoint_url());

        auto declaredPortEndpoints = declaredPorts[protocolUrl];
        if (declaredPortEndpoints.contains(protocolUrl))
            return AgentStatus::forCondition(AgentCondition::kInvalidConfiguration,
                "duplicate endpoint for declared port");

        declaredPortEndpoints.insert(endpointUrl);
    }

    // acquire mutex and ensure that the service is initialized
    absl::MutexLock lock(m_lock.get());
    if (m_supervisor == nullptr)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "service is not initialized");

    // respond to CreateMachine request and move machine to 'signing' queue
    auto waiter = std::make_shared<OnAgentSign>(reactor, response);
    TU_RETURN_IF_NOT_OK (m_supervisor->requestCertificates(machineName, *request, waiter));

    return {};
}

grpc::ServerUnaryReactor *
chord_agent::AgentService::SignCertificates(
    grpc::CallbackServerContext *context,
    const chord_invoke::SignCertificatesRequest *request,
    chord_invoke::SignCertificatesResult *response)
{
    TU_LOG_INFO << "SignCertificates request: " << request->DebugString();
    auto *reactor = context->DefaultReactor();
    auto status = doSignCertificates(reactor, context, request, response);
    if (status.notOk()) {
        reactor->Finish(chord_common::convert_status(status));
    }
    return reactor;
}

tempo_utils::Status
chord_agent::AgentService::doRunMachine(
    grpc::ServerUnaryReactor *reactor,
    grpc::CallbackServerContext *context,
    const chord_invoke::RunMachineRequest *request,
    chord_invoke::RunMachineResult *response)
{
    auto &machineName = request->machine_name();

    // acquire mutex and ensure that the service is initialized
    absl::MutexLock lock(m_lock.get());
    if (m_supervisor == nullptr)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "service is not initialized");

    // respond to SignCertificates request and move machine to 'ready' queue
    auto waiter = std::make_shared<OnAgentReady>(reactor, response);
    TU_RETURN_IF_NOT_OK (m_supervisor->bindCertificates(machineName, *request, waiter));

    return {};
}

grpc::ServerUnaryReactor *
chord_agent::AgentService::RunMachine(
    grpc::CallbackServerContext *context,
    const chord_invoke::RunMachineRequest *request,
    chord_invoke::RunMachineResult *response)
{
    TU_LOG_INFO << "RunMachine request: " << request->DebugString();
    auto *reactor = context->DefaultReactor();
    auto status = doRunMachine(reactor, context, request, response);
    if (status.notOk()) {
        reactor->Finish(chord_common::convert_status(status));
    }
    return reactor;
}

tempo_utils::Status
chord_agent::AgentService::doAdvertiseEndpoints(
    grpc::ServerUnaryReactor *reactor,
    grpc::CallbackServerContext *context,
    const chord_invoke::AdvertiseEndpointsRequest *request,
    chord_invoke::AdvertiseEndpointsResult *response)
{
    auto &machineName = request->machine_name();

    // acquire mutex and ensure that the service is initialized
    absl::MutexLock lock(m_lock.get());
    if (m_supervisor == nullptr)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "service is not initialized");

    // respond to RunMachine request
    TU_RETURN_IF_NOT_OK (m_supervisor->startMachine(machineName, *request));

    // complete the AdvertiseEndpoints request
    reactor->Finish(grpc::Status::OK);

    return {};
}

grpc::ServerUnaryReactor *
chord_agent::AgentService::AdvertiseEndpoints(
    grpc::CallbackServerContext *context,
    const chord_invoke::AdvertiseEndpointsRequest *request,
    chord_invoke::AdvertiseEndpointsResult *response)
{
    TU_LOG_INFO << "AdvertiseEndpoints request: " << request->SerializeAsString();
    auto *reactor = context->DefaultReactor();
    auto status = doAdvertiseEndpoints(reactor, context, request, response);
    if (status.notOk()) {
        reactor->Finish(chord_common::convert_status(status));
    }
    return reactor;
}

tempo_utils::Status
chord_agent::AgentService::doDeleteMachine(
    grpc::ServerUnaryReactor *reactor,
    grpc::CallbackServerContext *context,
    const chord_invoke::DeleteMachineRequest *request,
    chord_invoke::DeleteMachineResult *response)
{
    auto &machineName = request->machine_name();

    // acquire mutex and ensure that the service is initialized
    absl::MutexLock lock(m_lock.get());
    if (m_supervisor == nullptr)
        return AgentStatus::forCondition(AgentCondition::kAgentInvariant,
            "service is not initialized");

    auto waiter = std::make_shared<OnAgentTerminate>(reactor, response);
    TU_RETURN_IF_NOT_OK (m_supervisor->terminateMachine(machineName, waiter));

    return {};
}

grpc::ServerUnaryReactor *
chord_agent::AgentService::DeleteMachine(
    grpc::CallbackServerContext *context,
    const chord_invoke::DeleteMachineRequest *request,
    chord_invoke::DeleteMachineResult *response)
{
    TU_LOG_INFO << "DeleteMachine request: " << request->SerializeAsString();
    auto *reactor = context->DefaultReactor();
    auto status = doDeleteMachine(reactor, context, request, response);
    if (status.notOk()) {
        reactor->Finish(chord_common::convert_status(status));
    }
    return reactor;
}

chord_agent::OnAgentSpawn::OnAgentSpawn(grpc::ServerUnaryReactor *reactor, chord_invoke::CreateMachineResult *result)
    : m_reactor(reactor),
      m_result(result)
{
    TU_ASSERT (m_reactor != nullptr);
    TU_ASSERT (m_result != nullptr);
}

void
chord_agent::OnAgentSpawn::onComplete(
    MachineHandle handle,
    const chord_invoke::SignCertificatesRequest &signCertificatesRequest)
{
    // forward declared ports
    for (const auto &declaredPort : signCertificatesRequest.declared_ports()) {
        auto *resultPort = m_result->add_declared_ports();
        resultPort->set_protocol_url(declaredPort.protocol_url());
        resultPort->set_endpoint_index(declaredPort.endpoint_index());
        resultPort->set_port_type(declaredPort.port_type());
        resultPort->set_port_direction(declaredPort.port_direction());
    }

    // forward declared endpoints
    for (const auto &declaredEndpoint : signCertificatesRequest.declared_endpoints()) {
        auto *resultEndpoint = m_result->add_declared_endpoints();
        resultEndpoint->set_endpoint_url(declaredEndpoint.endpoint_url());
        resultEndpoint->set_csr(declaredEndpoint.csr());
    }

    m_reactor->Finish(grpc::Status::OK);
}

void
chord_agent::OnAgentSpawn::onStatus(tempo_utils::Status status)
{
    TU_LOG_INFO << "OnAgentSpawn failed: " << status.toString();
    m_reactor->Finish(grpc::Status(grpc::StatusCode::ABORTED, status.toString()));
}

chord_agent::OnAgentSign::OnAgentSign(grpc::ServerUnaryReactor *reactor, chord_invoke::SignCertificatesResult *result)
    : m_reactor(reactor),
      m_result(result)
{
    TU_ASSERT (m_reactor != nullptr);
    TU_ASSERT (m_result != nullptr);
}

void
chord_agent::OnAgentSign::onComplete(MachineHandle handle, const chord_invoke::RunMachineRequest &runMachineRequest)
{
    for (const auto &signedEndpoint : runMachineRequest.signed_endpoints()) {
        auto *resultEndpoint = m_result->add_signed_endpoints();
        resultEndpoint->set_endpoint_url(signedEndpoint.endpoint_url());
        resultEndpoint->set_certificate(signedEndpoint.certificate());
    }

    m_reactor->Finish(grpc::Status::OK);
}

void
chord_agent::OnAgentSign::onStatus(tempo_utils::Status status)
{
    TU_LOG_INFO << "OnAgentSign failed: " << status.toString();
    m_reactor->Finish(grpc::Status(grpc::StatusCode::ABORTED, status.toString()));
}

chord_agent::OnAgentReady::OnAgentReady(grpc::ServerUnaryReactor *reactor, chord_invoke::RunMachineResult *result)
    : m_reactor(reactor),
      m_result(result)
{
    TU_ASSERT (m_reactor != nullptr);
    TU_ASSERT (m_result != nullptr);
}

void
chord_agent::OnAgentReady::onComplete(
    MachineHandle handle,
    const chord_invoke::AdvertiseEndpointsRequest &advertiseEndpointsRequest)
{
    for (const auto &boundEndpoint : advertiseEndpointsRequest.bound_endpoints()) {
        auto *resultEndpoint = m_result->add_bound_endpoints();
        resultEndpoint->set_endpoint_url(boundEndpoint.endpoint_url());
    }

    m_reactor->Finish(grpc::Status::OK);
}

void
chord_agent::OnAgentReady::onStatus(tempo_utils::Status status)
{
    TU_LOG_INFO << "OnAgentReady failed: " << status.toString();
    m_reactor->Finish(grpc::Status(grpc::StatusCode::ABORTED, status.toString()));
}

chord_agent::OnAgentTerminate::OnAgentTerminate(
    grpc::ServerUnaryReactor *reactor,
    chord_invoke::DeleteMachineResult *result)
    : m_reactor(reactor),
      m_result(result)
{
    TU_ASSERT (m_reactor != nullptr);
    TU_ASSERT (m_result != nullptr);
}

void
chord_agent::OnAgentTerminate::onComplete(ExitStatus exitStatus)
{
    m_result->set_exit_status(exitStatus.status);
    m_reactor->Finish(grpc::Status::OK);
}

void
chord_agent::OnAgentTerminate::onStatus(tempo_utils::Status status)
{
    TU_LOG_INFO << "OnAgentTerminate failed: " << status.toString();
    m_reactor->Finish(grpc::Status(grpc::StatusCode::ABORTED, status.toString()));
}
