
#include <uv.h>

#include <chord_agent/agent_result.h>
#include <chord_agent/agent_service.h>
#include <chord_common/grpc_status.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/container_conversions.h>
#include <tempo_config/parse_config.h>
#include <tempo_utils/file_utilities.h>

chord_agent::AgentService::AgentService(
    MachineSupervisor *supervisor,
    std::string_view agentName,
    const std::filesystem::path &localMachineExecutable)
    : m_supervisor(supervisor),
      m_agentName(agentName),
      m_localMachineExecutable(localMachineExecutable)
{
    TU_ASSERT (m_supervisor != nullptr);
    TU_ASSERT (!m_agentName.empty());
    TU_ASSERT (!m_localMachineExecutable.empty());
    uv_timeval64_t tv;
    uv_gettimeofday(&tv);
    m_uptime = tv.tv_sec;
    m_lock = std::make_unique<absl::Mutex>();
}

std::string
chord_agent::AgentService::getListenTarget() const
{
    absl::MutexLock lock(m_lock.get());
    return m_listenTarget;
}

void
chord_agent::AgentService::setListenTarget(const std::string &listenTarget)
{
    absl::MutexLock lock(m_lock.get());
    m_listenTarget = listenTarget;
}

grpc::ServerUnaryReactor *
chord_agent::AgentService::IdentifyAgent(
    grpc::CallbackServerContext *context,
    const chord_invoke::IdentifyAgentRequest *request,
    chord_invoke::IdentifyAgentResult *response)
{
    uv_timeval64_t tv;
    uv_gettimeofday(&tv);
    response->set_agent_name(m_agentName);
    response->set_uptime_millis(tv.tv_sec - m_uptime);
    auto *reactor = context->DefaultReactor();
    reactor->Finish(grpc::Status::OK);
    return reactor;
}

static tempo_utils::Status
parse_config_hash(
    tempo_utils::ProcessBuilder &builder,
    const tempo_config::ConfigMap &config,
    const std::string &defaultAgentName)
{
    tempo_config::PathParser runDirectoryParser(std::filesystem::current_path());
    tempo_config::PathParser installDirectoryParser(std::filesystem::path{});
    tempo_config::PathParser packageDirectoryParser;
    tempo_config::SeqTParser packageDirectoriesParser(&packageDirectoryParser, {});
    tempo_config::StringParser agentServerNameParser(defaultAgentName);
    tempo_config::PathParser pemRootCABundleFileParser;
    tempo_config::PathParser pemCertificateFileParser(std::filesystem::path{});
    tempo_config::PathParser pemPrivateKeyFileParser(std::filesystem::path{});

    // determine the run directory
    std::filesystem::path runDirectory;
    TU_RETURN_IF_NOT_OK(tempo_config::parse_config(runDirectory, runDirectoryParser,
        config, "runDirectory"));
    if (!runDirectory.empty()) {
        builder.appendArg("--run-directory", runDirectory.string());
    }

    // determine the install directory
    std::filesystem::path installDirectory;
    TU_RETURN_IF_NOT_OK(tempo_config::parse_config(installDirectory, installDirectoryParser,
        config, "installDirectory"));
    if (!installDirectory.empty()) {
        builder.appendArg("--install-directory", installDirectory.string());
    }

    // determine the package directories
    std::vector<std::filesystem::path> packageDirectories;
    TU_RETURN_IF_NOT_OK(tempo_config::parse_config(packageDirectories, packageDirectoriesParser,
        config, "packageDirectories"));
    for (const auto &packageDirectory : packageDirectories) {
        builder.appendArg("--package-directory", packageDirectory.string());
    }

    // determine the agent server name
    std::string agentServerName;
    TU_RETURN_IF_NOT_OK(tempo_config::parse_config(agentServerName, agentServerNameParser,
        config, "agentServerName"));
    if (!agentServerName.empty()) {
        builder.appendArg("--supervisor-server-name", agentServerName);
    }

    // determine the pem root CA bundle file
    std::filesystem::path pemRootCABundleFile;
    TU_RETURN_IF_NOT_OK(tempo_config::parse_config(pemRootCABundleFile, pemRootCABundleFileParser,
        config, "pemRootCABundleFile"));
    builder.appendArg("--ca-bundle", pemRootCABundleFile.string());

    return {};
}

tempo_utils::Status
chord_agent::AgentService::doCreateMachine(
    grpc::ServerUnaryReactor *reactor,
    grpc::CallbackServerContext *context,
    const chord_invoke::CreateMachineRequest *request,
    chord_invoke::CreateMachineResult *response)
{
    tempo_config::ConfigNode config;
    TU_ASSIGN_OR_RETURN (config, tempo_config::read_config_string(request->config_hash()));

    if (config.getNodeType() != tempo_config::ConfigNodeType::kMap)
        return AgentStatus::forCondition(AgentCondition::kInvalidConfiguration,
            "invalid config hash");
    auto configMap = config.toMap();

    // construct the unique machine url
    auto machineName = absl::StrCat(tempo_utils::generate_name("chord-local-machine-XXXXXXXX"), ".test");
    auto machineUrl = tempo_utils::Url::fromString(absl::StrCat("https://", machineName));

    // append builder args based on the config hash
    tempo_utils::ProcessBuilder builder(m_localMachineExecutable);
    TU_RETURN_IF_NOT_OK (parse_config_hash(builder, configMap, m_agentName));

    // append expected port args
    for (const auto &requestedPort : request->requested_ports()) {
        builder.appendArg("--expected-port", requestedPort.protocol_url());
    }

    // append start-suspended flag
    if (request->start_suspended()) {
        builder.appendArg("--start-suspended");
    }

    // append the final required builder args
    builder.appendArg(m_listenTarget);
    builder.appendArg(request->execution_url());
    builder.appendArg(machineUrl.toString());

    // spawn the helper process and move machine to 'spawning' queue
    auto waiter = std::make_shared<OnAgentSpawn>(reactor, response);
    auto spawnMachineStatus = m_supervisor->spawnMachine(machineUrl, builder.toInvoker(), waiter);
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
    // verify machine url is valid
    auto machineUrl = tempo_utils::Url::fromString(request->machine_url());
    if (!machineUrl.isValid())
        return AgentStatus::forCondition(AgentCondition::kInvalidConfiguration,
            "invalid parameter machine_url");

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

    // respond to CreateMachine request and move machine to 'signing' queue
    auto waiter = std::make_shared<OnAgentSign>(reactor, response);
    TU_RETURN_IF_NOT_OK (m_supervisor->requestCertificates(machineUrl, *request, waiter));

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
    TU_LOG_INFO << "RunMachine request: " << request->SerializeAsString();

    // verify machine url is valid
    auto machineUrl = tempo_utils::Url::fromString(request->machine_url());
    if (!machineUrl.isValid())
        return AgentStatus::forCondition(AgentCondition::kInvalidConfiguration,
            "invalid parameter machine_url");

    // respond to SignCertificates request and move machine to 'ready' queue
    auto waiter = std::make_shared<OnAgentReady>(reactor, response);
    TU_RETURN_IF_NOT_OK (m_supervisor->bindCertificates(machineUrl, *request, waiter));

    return {};
}

grpc::ServerUnaryReactor *
chord_agent::AgentService::RunMachine(
    grpc::CallbackServerContext *context,
    const chord_invoke::RunMachineRequest *request,
    chord_invoke::RunMachineResult *response)
{
    TU_LOG_INFO << "SignCertificates request: " << request->DebugString();
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
    // verify machine url is valid
    auto machineUrl = tempo_utils::Url::fromString(request->machine_url());
    if (!machineUrl.isValid())
        return AgentStatus::forCondition(AgentCondition::kInvalidConfiguration,
            "invalid parameter machine_url");

    // respond to RunMachine request
    TU_LOG_INFO << "calling bind()";
    TU_RETURN_IF_NOT_OK (m_supervisor->startMachine(machineUrl, *request));

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
    // verify machine url is valid
    auto machineUrl = tempo_utils::Url::fromString(request->machine_url());
    if (!machineUrl.isValid())
        return AgentStatus::forCondition(AgentCondition::kInvalidConfiguration,
            "invalid parameter machine_url");

    auto waiter = std::make_shared<OnAgentTerminate>(reactor, response);
    TU_RETURN_IF_NOT_OK (m_supervisor->terminateMachine(machineUrl, waiter));

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
    m_result->set_machine_url(handle.url.toString());

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
