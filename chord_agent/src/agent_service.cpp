
#include <boost/uuid/uuid_io.hpp>
#include <uv.h>

#include <chord_agent/agent_service.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/container_conversions.h>
#include <tempo_config/parse_config.h>
#include <tempo_utils/file_utilities.h>

AgentService::AgentService(
    const std::string &listenEndpoint,
    MachineSupervisor *supervisor,
    std::string_view agentName,
    const std::filesystem::path &localMachineExecutable)
    : m_listenEndpoint(listenEndpoint),
      m_supervisor(supervisor),
      m_agentName(agentName),
      m_localMachineExecutable(localMachineExecutable)
{
    TU_ASSERT (!m_listenEndpoint.empty());
    TU_ASSERT (m_supervisor != nullptr);
    TU_ASSERT (!m_localMachineExecutable.empty());
    uv_timeval64_t tv;
    uv_gettimeofday(&tv);
    m_uptime = tv.tv_sec;
}

grpc::ServerUnaryReactor *
AgentService::AgentService::IdentifyAgent(
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
    tempo_config::SeqTParser<std::filesystem::path> packageDirectoriesParser(&packageDirectoryParser, {});
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

    // determine the pem certificate file
    std::filesystem::path pemCertificateFile;
    TU_RETURN_IF_NOT_OK(tempo_config::parse_config(pemCertificateFile, pemCertificateFileParser,
        config, "pemCertificateFile"));
    if (!pemCertificateFile.empty()) {
        builder.appendArg("--certificate", pemCertificateFile.string());
    }

    // determine the pem private key file
    std::filesystem::path pemPrivateKeyFile;
    TU_RETURN_IF_NOT_OK(tempo_config::parse_config(pemPrivateKeyFile, pemPrivateKeyFileParser,
        config, "pemPrivateKeyFile"));
    if (!pemPrivateKeyFile.empty()) {
        builder.appendArg("--private-key", pemPrivateKeyFile.string());
    }

    // determine the pem root CA bundle file
    std::filesystem::path pemRootCABundleFile;
    TU_RETURN_IF_NOT_OK(tempo_config::parse_config(pemRootCABundleFile, pemRootCABundleFileParser,
        config, "pemRootCABundleFile"));
    builder.appendArg("--ca-bundle", pemRootCABundleFile.string());

    return tempo_config::ConfigStatus::ok();
}

grpc::ServerUnaryReactor *
AgentService::CreateMachine(
    grpc::CallbackServerContext *context,
    const chord_invoke::CreateMachineRequest *request,
    chord_invoke::CreateMachineResult *response)
{
    TU_LOG_INFO << "CreateMachine request: " << request->DebugString();
    auto *reactor = context->DefaultReactor();

    auto parseConfigHashResult = tempo_config::read_config_string(request->config_hash());
    if (parseConfigHashResult.isStatus()) {
        grpc::Status status(grpc::StatusCode::INVALID_ARGUMENT, parseConfigHashResult.getStatus().toString());
        TU_LOG_INFO << "CreateMachine failed: " << status.error_message();
        reactor->Finish(status);
        return reactor;
    }
    auto config = parseConfigHashResult.getResult();

    if (config.getNodeType() != tempo_config::ConfigNodeType::kMap) {
        grpc::Status status(grpc::StatusCode::INVALID_ARGUMENT, "invalid config hash");
        TU_LOG_INFO << "CreateMachine failed: " << status.error_message();
        reactor->Finish(status);
        return reactor;
    }
    auto configMap = config.toMap();

    // construct the unique machine url
    auto machineName = absl::StrCat(tempo_utils::generate_name("chord-local-machine-XXXXXXXX"), ".test");
    auto machineUrl = tempo_utils::Url::fromString(absl::StrCat("https://", machineName));

    // append builder args based on the config hash
    tempo_utils::ProcessBuilder builder(m_localMachineExecutable);
    auto parseConfigMapStatus = parse_config_hash(builder, configMap, m_agentName);
    if (parseConfigMapStatus.notOk()) {
        grpc::Status status(grpc::StatusCode::INVALID_ARGUMENT, parseConfigMapStatus.toString());
        TU_LOG_INFO << "CreateMachine failed: " << status.error_message();
        reactor->Finish(status);
        return reactor;
    }

    // append expected port args
    for (const auto &requestedPort : request->requested_ports()) {
        builder.appendArg("--expected-port", requestedPort.protocol_url());
    }

    // append start-suspended flag
    if (request->start_suspended()) {
        builder.appendArg("--start-suspended");
    }

    // append the final required builder args
    builder.appendArg(m_listenEndpoint);
    builder.appendArg(request->execution_url());
    builder.appendArg(machineUrl.toString());

    // spawn the helper process and move machine to 'spawning' queue
    auto waiter = std::make_shared<OnAgentSpawn>(reactor, response);
    auto spawnMachineStatus = m_supervisor->spawnMachine(machineUrl, builder.toInvoker(), waiter);
    if (spawnMachineStatus.notOk()) {
        waiter->onStatus(spawnMachineStatus);
    }

    return reactor;
}

grpc::ServerUnaryReactor *
AgentService::SignCertificates(
    grpc::CallbackServerContext *context,
    const chord_invoke::SignCertificatesRequest *request,
    chord_invoke::SignCertificatesResult *response)
{
    TU_LOG_INFO << "SignCertificates request: " << request->DebugString();
    auto *reactor = context->DefaultReactor();

    // verify machine url is valid
    auto machineUrl = tempo_utils::Url::fromString(request->machine_url());
    if (!machineUrl.isValid()) {
        grpc::Status status(grpc::StatusCode::INVALID_ARGUMENT, "invalid parameter machine_url");
        TU_LOG_INFO << "SignCertificates failed: " << status.error_message();
        reactor->Finish(status);
        return reactor;
    }

    // verify any duplicate declared ports each have a unique endpoint
    absl::flat_hash_map<tempo_utils::Url, absl::flat_hash_set<tempo_utils::Url>> declaredPorts;
    for (const auto &declaredPort : request->declared_ports()) {
        auto protocolUrl = tempo_utils::Url::fromString(declaredPort.protocol_url());
        if (request->declared_endpoints_size() <= declaredPort.endpoint_index()) {
            grpc::Status status(grpc::StatusCode::INVALID_ARGUMENT, "invalid endpoint index for declared port");
            TU_LOG_INFO << "SignCertificates failed: " << status.error_message();
            reactor->Finish(status);
            return reactor;
        }

        const auto &declaredEndpoint = request->declared_endpoints(declaredPort.endpoint_index());
        auto endpointUrl = tempo_utils::Url::fromString(declaredEndpoint.endpoint_url());

        auto declaredPortEndpoints = declaredPorts[protocolUrl];
        if (declaredPortEndpoints.contains(protocolUrl)) {
            grpc::Status status(grpc::StatusCode::INVALID_ARGUMENT, "duplicate endpoint for declared port");
            TU_LOG_INFO << "SignCertificates failed: " << status.error_message();
            reactor->Finish(status);
            return reactor;
        }

        declaredPortEndpoints.insert(endpointUrl);
    }

    // respond to CreateMachine request and move machine to 'signing' queue
    auto waiter = std::make_shared<OnAgentSign>(reactor, response);
    auto requestCertificatesStatus = m_supervisor->requestCertificates(machineUrl, *request, waiter);
    if (requestCertificatesStatus.notOk()) {
        grpc::Status status(grpc::StatusCode::INTERNAL, requestCertificatesStatus.toString());
        TU_LOG_INFO << "SignCertificates failed: " << status.error_message();
        reactor->Finish(status);
        return reactor;
    }

    return reactor;
}

grpc::ServerUnaryReactor *
AgentService::RunMachine(
    grpc::CallbackServerContext *context,
    const chord_invoke::RunMachineRequest *request,
    chord_invoke::RunMachineResult *response)
{
    TU_LOG_INFO << "RunMachine request: " << request->SerializeAsString();
    auto *reactor = context->DefaultReactor();

    // verify machine url is valid
    auto machineUrl = tempo_utils::Url::fromString(request->machine_url());
    if (!machineUrl.isValid()) {
        grpc::Status status(grpc::StatusCode::INVALID_ARGUMENT, "invalid parameter machine_url");
        TU_LOG_INFO << "RunMachine failed: " << status.error_message();
        reactor->Finish(status);
        return reactor;
    }

    // respond to SignCertificates request and move machine to 'ready' queue
    auto waiter = std::make_shared<OnAgentReady>(reactor, response);
    auto bindCertificatesStatus = m_supervisor->bindCertificates(machineUrl, *request, waiter);
    if (bindCertificatesStatus.notOk()) {
        grpc::Status status(grpc::StatusCode::INTERNAL, bindCertificatesStatus.toString());
        TU_LOG_INFO << "RunMachine failed: " << status.error_message();
        reactor->Finish(status);
        return reactor;
    }

    return reactor;
}

grpc::ServerUnaryReactor *
AgentService::AdvertiseEndpoints(
    grpc::CallbackServerContext *context,
    const chord_invoke::AdvertiseEndpointsRequest *request,
    chord_invoke::AdvertiseEndpointsResult *response)
{
    TU_LOG_INFO << "AdvertiseEndpoints request: " << request->SerializeAsString();
    auto *reactor = context->DefaultReactor();

    // verify machine url is valid
    auto machineUrl = tempo_utils::Url::fromString(request->machine_url());
    if (!machineUrl.isValid()) {
        grpc::Status status(grpc::StatusCode::INVALID_ARGUMENT, "invalid parameter machine_url");
        TU_LOG_INFO << "AdvertiseEndpoints failed: " << status.error_message();
        reactor->Finish(status);
        return reactor;
    }

    // respond to RunMachine request
    TU_LOG_INFO << "calling bind()";
    auto startMachineStatus = m_supervisor->startMachine(machineUrl, *request);
    if (startMachineStatus.notOk()) {
        grpc::Status status(grpc::StatusCode::INTERNAL, startMachineStatus.toString());
        TU_LOG_INFO << "AdvertiseEndpoints failed: " << status.error_message();
        reactor->Finish(status);
        return reactor;
    }

    // complete the AdvertiseEndpoints request
    reactor->Finish(grpc::Status::OK);
    return reactor;
}

grpc::ServerUnaryReactor *
AgentService::DeleteMachine(
    grpc::CallbackServerContext *context,
    const chord_invoke::DeleteMachineRequest *request,
    chord_invoke::DeleteMachineResult *response)
{
    TU_LOG_INFO << "DeleteMachine request: " << request->SerializeAsString();
    auto *reactor = context->DefaultReactor();

    // verify machine url is valid
    auto machineUrl = tempo_utils::Url::fromString(request->machine_url());
    if (!machineUrl.isValid()) {
        grpc::Status status(grpc::StatusCode::INVALID_ARGUMENT, "invalid parameter machine_url");
        TU_LOG_INFO << "DeleteMachine failed: " << status.error_message();
        reactor->Finish(status);
        return reactor;
    }

    auto waiter = std::make_shared<OnAgentTerminate>(reactor, response);
    auto terminateMachineStatus = m_supervisor->terminateMachine(machineUrl, waiter);
    if (terminateMachineStatus.notOk()) {
        grpc::Status status(grpc::StatusCode::INTERNAL, terminateMachineStatus.toString());
        TU_LOG_INFO << "DeleteMachine failed: " << status.error_message();
        reactor->Finish(status);
        return reactor;
    }

    return reactor;
}

OnAgentSpawn::OnAgentSpawn(grpc::ServerUnaryReactor *reactor, chord_invoke::CreateMachineResult *result)
    : m_reactor(reactor),
      m_result(result)
{
    TU_ASSERT (m_reactor != nullptr);
    TU_ASSERT (m_result != nullptr);
}

void
OnAgentSpawn::onComplete(
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
OnAgentSpawn::onStatus(tempo_utils::Status status)
{
    TU_LOG_INFO << "OnAgentSpawn failed: " << status.toString();
    m_reactor->Finish(grpc::Status(grpc::StatusCode::ABORTED, status.toString()));
}

OnAgentSign::OnAgentSign(grpc::ServerUnaryReactor *reactor, chord_invoke::SignCertificatesResult *result)
    : m_reactor(reactor),
      m_result(result)
{
    TU_ASSERT (m_reactor != nullptr);
    TU_ASSERT (m_result != nullptr);
}

void
OnAgentSign::onComplete(MachineHandle handle, const chord_invoke::RunMachineRequest &runMachineRequest)
{
    for (const auto &signedEndpoint : runMachineRequest.signed_endpoints()) {
        auto *resultEndpoint = m_result->add_signed_endpoints();
        resultEndpoint->set_endpoint_url(signedEndpoint.endpoint_url());
        resultEndpoint->set_certificate(signedEndpoint.certificate());
    }

    m_reactor->Finish(grpc::Status::OK);
}

void
OnAgentSign::onStatus(tempo_utils::Status status)
{
    TU_LOG_INFO << "OnAgentSign failed: " << status.toString();
    m_reactor->Finish(grpc::Status(grpc::StatusCode::ABORTED, status.toString()));
}

OnAgentReady::OnAgentReady(grpc::ServerUnaryReactor *reactor, chord_invoke::RunMachineResult *result)
    : m_reactor(reactor),
      m_result(result)
{
    TU_ASSERT (m_reactor != nullptr);
    TU_ASSERT (m_result != nullptr);
}

void
OnAgentReady::onComplete(
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
OnAgentReady::onStatus(tempo_utils::Status status)
{
    TU_LOG_INFO << "OnAgentReady failed: " << status.toString();
    m_reactor->Finish(grpc::Status(grpc::StatusCode::ABORTED, status.toString()));
}

OnAgentTerminate::OnAgentTerminate(
    grpc::ServerUnaryReactor *reactor,
    chord_invoke::DeleteMachineResult *result)
    : m_reactor(reactor),
      m_result(result)
{
    TU_ASSERT (m_reactor != nullptr);
    TU_ASSERT (m_result != nullptr);
}

void
OnAgentTerminate::onComplete(ExitStatus exitStatus)
{
    m_result->set_exit_status(exitStatus.status);
    m_reactor->Finish(grpc::Status::OK);
}

void
OnAgentTerminate::onStatus(tempo_utils::Status status)
{
    TU_LOG_INFO << "OnAgentTerminate failed: " << status.toString();
    m_reactor->Finish(grpc::Status(grpc::StatusCode::ABORTED, status.toString()));
}
