#include <sys/wait.h>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/ascii.h>

#include <chord_invoke/invoke_service.grpc.pb.h>
#include <chord_sandbox/internal/machine_utils.h>
#include <chord_sandbox/internal/spawn_utils.h>
#include <chord_sandbox/run_protocol_plug.h>
#include <grpcpp/create_channel.h>
#include <tempo_config/config_serde.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_security/x509_certificate_signing_request.h>
#include <tempo_utils/daemon_process.h>
#include <tempo_utils/file_reader.h>
#include <tempo_utils/log_stream.h>
#include <tempo_utils/process_utils.h>
#include <tempo_utils/url.h>

namespace chord_sandbox {
    struct SandboxPriv {
        std::unique_ptr<chord_invoke::InvokeService::Stub> stub;
    };
}

#include <chord_sandbox/chord_isolate.h>
#include <chord_sandbox/grpc_connector.h>

chord_sandbox::ChordIsolate::ChordIsolate()
{
}

chord_sandbox::ChordIsolate::ChordIsolate(const SandboxOptions &options)
    : m_options(options)
{
}

chord_sandbox::ChordIsolate::~ChordIsolate()
{
}

static std::shared_ptr<tempo_utils::DaemonProcess>
spawn_func(const tempo_utils::ProcessInvoker &invoker, const std::filesystem::path &runDirectory)
{
    return tempo_utils::DaemonProcess::spawn(invoker, runDirectory);
}

tempo_utils::Status
chord_sandbox::ChordIsolate::initialize()
{
    if (m_channel != nullptr)
        return SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "sandbox is already initialized");

    // spawn new agent or connect to existing agent, depending on policy
    chord_sandbox::internal::AgentParams params;
    switch (m_options.discoveryPolicy) {
        case AgentDiscoveryPolicy::USE_SPECIFIED_ENDPOINT: {
            auto status = chord_sandbox::internal::connect_to_specified_endpoint(
                params,
                m_options.agentEndpoint,
                m_options.agentServerName,
                m_options.pemRootCABundleFile,
                spawn_func);
            if (status.notOk())
                return status;
            break;
        }
        case AgentDiscoveryPolicy::SPAWN_IF_MISSING: {
            auto status = chord_sandbox::internal::spawn_temporary_agent_if_missing(
                params,
                m_options.agentEndpoint,
                m_options.agentPath,
                m_options.endpointTransport,
                m_options.agentServerName,
                m_options.runDirectory,
                m_options.agentKeyPair.getPemCertificateFile(),
                m_options.agentKeyPair.getPemPrivateKeyFile(),
                m_options.pemRootCABundleFile,
                spawn_func);
            if (status.notOk())
                return status;
            break;
        }
        case AgentDiscoveryPolicy::ALWAYS_SPAWN: {
            auto status = chord_sandbox::internal::spawn_temporary_agent(
                params,
                m_options.agentPath,
                m_options.endpointTransport,
                m_options.agentServerName,
                m_options.runDirectory,
                m_options.agentKeyPair.getPemCertificateFile(),
                m_options.agentKeyPair.getPemPrivateKeyFile(),
                m_options.pemRootCABundleFile,
                spawn_func);
            if (status.notOk())
                return status;
            break;
        }
    }

    // construct the channel credentials
    grpc::SslCredentialsOptions options;
    tempo_utils::FileReader rootCABundleReader(params.pemRootCABundleFile);
    if (!rootCABundleReader.isValid())
        return SandboxStatus::forCondition(SandboxCondition::kInvalidConfiguration,
            "failed to read root CA bundle {}", params.pemRootCABundleFile.string());
    auto rootCABytes = rootCABundleReader.getBytes();
    options.pem_root_certs = std::string((const char *) rootCABytes->getData(), rootCABytes->getSize());
    auto credentials = grpc::SslCredentials(options);

    // construct the client
    grpc::ChannelArguments channelArguments;
    if (!params.agentServerName.empty())
        channelArguments.SetSslTargetNameOverride(params.agentServerName);
    m_channel = grpc::CreateCustomChannel(params.agentEndpoint, credentials, channelArguments);
    m_priv = std::make_unique<SandboxPriv>();
    m_priv->stub = chord_invoke::InvokeService::NewStub(m_channel);

    // verify that the agent is running
    grpc::ClientContext context;
    chord_invoke::IdentifyAgentRequest identifyRequest;
    chord_invoke::IdentifyAgentResult identifyResult;
    auto status = m_priv->stub->IdentifyAgent(&context, identifyRequest, &identifyResult);
    if (!status.ok())
        return SandboxStatus::forCondition(SandboxCondition::kAgentError,
            "IdentifyAgent failed: {}", status.error_message());

    TU_LOG_INFO << "connected to agent " << identifyResult.agent_name();

    return SandboxStatus::ok();
}

tempo_utils::Result<std::shared_ptr<chord_sandbox::RemoteMachine>>
chord_sandbox::ChordIsolate::spawn(
    std::string_view name,
    const lyric_common::AssemblyLocation &mainLocation,
    const tempo_config::ConfigMap &configMap,
    const absl::flat_hash_set<chord_protocol::RequestedPort> &requestedPorts,
    const absl::flat_hash_map<tempo_utils::Url,std::shared_ptr<chord_protocol::AbstractProtocolHandler>> &plugs,
    RunProtocolCallback cb,
    void *cbData)
{
    auto zuriRunProtocolUrl = tempo_utils::Url::fromString(kRunProtocolUri);
    if (plugs.contains(zuriRunProtocolUrl))
        return SandboxStatus::forCondition(SandboxCondition::kInvalidPlug,
            "plug {} is managed automatically", zuriRunProtocolUrl.toString());

    // copy the given plugs map and insert the run plug
    absl::flat_hash_map<
        tempo_utils::Url,
        std::shared_ptr<chord_protocol::AbstractProtocolHandler>> plugsMap = plugs;
    auto runPlug = std::make_shared<RunProtocolPlug>(cb, cbData);
    plugsMap[zuriRunProtocolUrl] = runPlug;

    // copy the given requested ports set and insert the run port
    absl::flat_hash_set<chord_protocol::RequestedPort> requestedPortsSet = requestedPorts;
    requestedPortsSet.insert(chord_protocol::RequestedPort(zuriRunProtocolUrl,
        chord_protocol::PortType::Streaming, chord_protocol::PortDirection::BiDirectional));

    // verify that each requested port has a plug implementing the protocol
    for (const auto &port : requestedPortsSet) {
        if (!plugsMap.contains(port.getUrl()))
            return SandboxStatus::forCondition( SandboxCondition::kInvalidConfiguration,
                "requested port {} has no plug", port.getUrl().toString());
    }

    // call CreateMachine on the agent endpoint
    chord_invoke::CreateMachineResult createMachineResult;
    TU_ASSIGN_OR_RETURN (createMachineResult, internal::create_machine(m_priv->stub.get(),
        name, mainLocation.toUrl(), configMap, requestedPortsSet));

    auto machineUrl = tempo_utils::Url::fromString(createMachineResult.machine_uri());
    auto machineHostAndPort = machineUrl.getHostAndPort();

    // construct a map of declared endpoint to csr
    absl::flat_hash_map<std::string,std::string> declaredEndpointCsrs;
    for (const auto &declaredEndpoint : createMachineResult.declared_endpoints()) {
        const auto &endpointUrl = declaredEndpoint.endpoint_uri();
        if (declaredEndpointCsrs.contains(endpointUrl))
            return SandboxStatus::forCondition( SandboxCondition::kInvalidConfiguration,
                "duplicate declared endpoint {}", endpointUrl);
        declaredEndpointCsrs[endpointUrl] = declaredEndpoint.csr();
    }

    // call RunMachine on the agent endpoint
    chord_invoke::RunMachineResult runMachineResult;
    TU_ASSIGN_OR_RETURN (runMachineResult, internal::run_machine(m_priv->stub.get(),
        machineUrl, declaredEndpointCsrs, m_options.caKeyPair, std::chrono::seconds(3600)));

    // create the connector
    auto connector = std::make_shared<GrpcConnector>(machineUrl, lyric_common::RuntimePolicy());

    // register plugs with the connector
    for (const auto &declaredPort : createMachineResult.declared_ports()) {
        auto protocolUrl = tempo_utils::Url::fromString(declaredPort.protocol_uri());
        if (!protocolUrl.isValid())
            return SandboxStatus::forCondition(SandboxCondition::kInvalidPort,
                "invalid declared port {}", protocolUrl.toString());

        if (plugsMap.contains(protocolUrl)) {
            auto endpointIndex = declaredPort.endpoint_index();
            if (createMachineResult.declared_endpoints_size() <= endpointIndex)
                return SandboxStatus::forCondition(SandboxCondition::kInvalidPort,
                    "invalid endpoint index {} for declared port {}", endpointIndex, protocolUrl.toString());
            auto &declaredEndpoint = createMachineResult.declared_endpoints((int) declaredPort.endpoint_index());
            auto endpointUrl = tempo_utils::Url::fromString(declaredEndpoint.endpoint_uri());

            auto handler = plugsMap.at(protocolUrl);
            TU_RETURN_IF_NOT_OK (connector->registerProtocolHandler(protocolUrl, handler, endpointUrl,
                m_options.pemRootCABundleFile, machineHostAndPort));
            TU_LOG_INFO << "registering handler for protocol " << protocolUrl << " using endpoint " << endpointUrl;
        } else {
            TU_LOG_WARN << "ignoring protocol " << protocolUrl << " because there is no registered plug";
        }
    }

    // create the machine and return it
    auto machine = std::make_shared<RemoteMachine>(name, mainLocation, machineUrl, connector, runPlug);
    m_machines[machineUrl] = machine;
    return machine;
}

tempo_utils::Status
chord_sandbox::ChordIsolate::shutdown()
{
    if (m_priv == nullptr)
        return SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "sandbox is not initialized");
    m_priv.reset();
    m_channel.reset();
    return SandboxStatus::ok();
}
