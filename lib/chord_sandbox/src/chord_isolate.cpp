#include <sys/wait.h>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/ascii.h>

#include <chord_invoke/invoke_service.grpc.pb.h>
#include <chord_sandbox/internal/machine_utils.h>
#include <chord_sandbox/internal/spawn_utils.h>
#include <chord_sandbox/run_protocol_plug.h>
#include <grpcpp/create_channel.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_security/x509_certificate_signing_request.h>
#include <tempo_utils/file_reader.h>
#include <tempo_utils/log_stream.h>
#include <tempo_utils/process_builder.h>
#include <tempo_utils/process_runner.h>
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
            TU_RETURN_IF_NOT_OK (internal::connect_to_specified_endpoint(
                params,
                m_options.agentEndpoint,
                m_options.agentServerName,
                m_options.pemRootCABundleFile));
            break;
        }
        case AgentDiscoveryPolicy::SPAWN_IF_MISSING: {
            TU_RETURN_IF_NOT_OK (internal::spawn_temporary_agent_if_missing(
                params,
                m_options.agentEndpoint,
                m_options.agentPath,
                m_options.endpointTransport,
                m_options.agentServerName,
                m_options.runDirectory,
                m_options.agentKeyPair.getPemCertificateFile(),
                m_options.agentKeyPair.getPemPrivateKeyFile(),
                m_options.pemRootCABundleFile));
            break;
        }
        case AgentDiscoveryPolicy::ALWAYS_SPAWN: {
            TU_RETURN_IF_NOT_OK (internal::spawn_temporary_agent(
                params,
                m_options.agentPath,
                m_options.agentServerName,
                m_options.runDirectory,
                m_options.idleTimeout,
                m_options.agentKeyPair.getPemCertificateFile(),
                m_options.agentKeyPair.getPemPrivateKeyFile(),
                m_options.pemRootCABundleFile));
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
    if (!params.agentServerName.empty()) {
        channelArguments.SetSslTargetNameOverride(params.agentServerName);
    }
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

    return {};
}

tempo_utils::Result<std::shared_ptr<chord_sandbox::RemoteMachine>>
chord_sandbox::ChordIsolate::spawn(
    std::string_view name,
    const tempo_utils::Url &mainLocation,
    const tempo_config::ConfigMap &configMap,
    const std::vector<RequestedPortAndHandler> &plugs,
    bool startSuspended)
{
    TU_ASSERT (!name.empty());
    TU_ASSERT (mainLocation.isValid());

    absl::flat_hash_set<chord_protocol::RequestedPort> requestedPortsSet;
    absl::flat_hash_map<tempo_utils::Url,std::shared_ptr<chord_protocol::AbstractProtocolHandler>> plugHandlersMap;

    // extract requested port and plug handler from plugs list
    for (const auto &plug : plugs) {
        const auto &requestedPort = plug.requestedPort;
        const auto protocolUrl = requestedPort.getUrl();
        if (plug.handler == nullptr)
            return SandboxStatus::forCondition( SandboxCondition::kInvalidConfiguration,
                "requested port {} has invalid handler", protocolUrl.toString());
        if (plugHandlersMap.contains(protocolUrl))
            return SandboxStatus::forCondition( SandboxCondition::kInvalidConfiguration,
                "requested port {} was already specified", protocolUrl.toString());
        plugHandlersMap[protocolUrl] = plug.handler;
        requestedPortsSet.insert(requestedPort);
    }

    // call CreateMachine on the agent endpoint
    internal::CreateMachineResult createMachineResult;
    TU_ASSIGN_OR_RETURN (createMachineResult, internal::create_machine(m_priv->stub.get(),
        name, mainLocation, configMap, requestedPortsSet, /* startSuspended= */ true));

    // call RunMachine on the agent endpoint
    internal::RunMachineResult runMachineResult;
    TU_ASSIGN_OR_RETURN (runMachineResult, internal::run_machine(m_priv->stub.get(),
        createMachineResult.machineUrl, createMachineResult.protocolEndpoints, createMachineResult.endpointCsrs,
        m_options.caKeyPair, std::chrono::seconds(3600)));

    // create the connector
    auto connector = std::make_shared<GrpcConnector>(
        createMachineResult.machineUrl, lyric_common::RuntimePolicy());

    // register plugs with the connector
    for (const auto &entry : createMachineResult.protocolEndpoints) {
        auto protocolUrl = entry.first;
        TU_ASSERT (protocolUrl.isValid());

        if (!plugHandlersMap.contains(protocolUrl))
            return SandboxStatus::forCondition(SandboxCondition::kInvalidPort,
                "no registered plug for protocol {}", protocolUrl.toString());
        auto &handler = plugHandlersMap.at(protocolUrl);

        // get the endpoint name override
        auto &endpointUrl = entry.second;
        std::string nameOverride;
        if (runMachineResult.endpointNameOverrides.contains(endpointUrl)) {
            nameOverride = runMachineResult.endpointNameOverrides.at(endpointUrl);
        }

        TU_RETURN_IF_NOT_OK (connector->registerProtocolHandler(protocolUrl, handler, endpointUrl,
            m_options.pemRootCABundleFile, nameOverride));
        TU_LOG_INFO << "registering handler for protocol " << protocolUrl << " using endpoint " << endpointUrl;
    }

    // get the control name override
    auto &controlUrl = createMachineResult.controlUrl;
    std::string nameOverride;
    if (runMachineResult.endpointNameOverrides.contains(controlUrl)) {
        nameOverride = runMachineResult.endpointNameOverrides.at(controlUrl);
    }

    // connect to the control and remoting endpoints
    TU_RETURN_IF_NOT_OK (connector->connect(createMachineResult.controlUrl,
        m_options.pemRootCABundleFile, nameOverride));

    // create the remote machine
    auto &machineUrl = createMachineResult.machineUrl;
    auto machine = std::make_shared<RemoteMachine>(name, mainLocation, machineUrl, connector);
    m_machines[machineUrl] = machine;

    // the remote machine is suspended, so if startSuspended is false then resume the machine
    if (!startSuspended) {
        TU_RETURN_IF_NOT_OK (machine->resume());
    }

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
    return {};
}
