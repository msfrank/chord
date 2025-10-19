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

#include "chord_sandbox/local_endpoint_signer.h"

struct chord_sandbox::ChordIsolate::SandboxPriv {
    std::unique_ptr<chord_invoke::InvokeService::Stub> stub;
};

#include <chord_sandbox/chord_isolate.h>
#include <chord_sandbox/grpc_connector.h>

static std::string
transport_location_to_grpc_target(const chord_common::TransportLocation &location)
{
    switch (location.getType()) {
        case chord_common::TransportType::Unix: {
            auto path = location.getUnixPath();
            return absl::StrCat("unix://", std::filesystem::absolute(path).string());
        }
        case chord_common::TransportType::Tcp4: {
            std::string target;
            auto address = location.getTcp4Address();
            if (std::isdigit(address.front())) {
                target = absl::StrCat("ipv4:", address);
            } else {
                target = absl::StrCat("dns:///", address);
            }
            auto portNumber = location.getTcp4Port();
            if (portNumber > 0) {
                absl::StrAppend(&target, ":", portNumber);
            }
            return target;
        }
        default:
            return {};
    }
}

tempo_utils::Result<std::shared_ptr<chord_sandbox::ChordIsolate>>
chord_sandbox::ChordIsolate::connect(
    std::string_view agentName,
    const chord_common::TransportLocation &agentEndpoint,
    std::shared_ptr<const chord_tooling::SecurityConfig> securityConfig,
    const IsolateOptions &options)
{
    auto &pemRootCABundleFile = securityConfig->pemRootCABundleFile;

    std::shared_ptr<AbstractEndpointSigner> endpointSigner;
    if (options.endpointSigner == nullptr) {
        tempo_security::CertificateKeyPair localCAKeypair;
        TU_ASSIGN_OR_RETURN (localCAKeypair, securityConfig->getSigningKeypair());
        endpointSigner = std::make_shared<LocalEndpointSigner>(localCAKeypair);
    } else {
        endpointSigner = options.endpointSigner;
    }

    auto target = transport_location_to_grpc_target(agentEndpoint);

    // construct the channel credentials
    grpc::SslCredentialsOptions sslCredentialsOptions;
    tempo_utils::FileReader rootCABundleReader(pemRootCABundleFile);
    if (!rootCABundleReader.isValid())
        return SandboxStatus::forCondition(SandboxCondition::kInvalidConfiguration,
            "failed to read root CA bundle {}", pemRootCABundleFile.string());
    auto rootCABytes = rootCABundleReader.getBytes();
    sslCredentialsOptions.pem_root_certs = std::string((const char *) rootCABytes->getData(), rootCABytes->getSize());
    auto credentials = grpc::SslCredentials(sslCredentialsOptions);

    // construct the client
    grpc::ChannelArguments channelArguments;
    if (!options.agentServerNameOverride.empty()) {
        channelArguments.SetSslTargetNameOverride(options.agentServerNameOverride);
    }

    auto channel = grpc::CreateCustomChannel(target, credentials, channelArguments);
    auto priv = std::make_unique<SandboxPriv>();
    priv->stub = chord_invoke::InvokeService::NewStub(channel);

    // verify that the agent is running
    grpc::ClientContext context;
    chord_invoke::IdentifyAgentRequest identifyRequest;
    chord_invoke::IdentifyAgentResult identifyResult;
    auto status = priv->stub->IdentifyAgent(&context, identifyRequest, &identifyResult);
    if (!status.ok())
        return SandboxStatus::forCondition(SandboxCondition::kAgentError,
            "IdentifyAgent failed: {}", status.error_message());

    TU_LOG_INFO << "connected to agent " << identifyResult.agent_name();

    auto isolate = std::shared_ptr<ChordIsolate>(new ChordIsolate(
        agentName, pemRootCABundleFile, endpointSigner, channel, std::move(priv)));
    return isolate;
}

tempo_utils::Result<std::shared_ptr<chord_sandbox::ChordIsolate>>
chord_sandbox::ChordIsolate::connect(
    std::string_view agentName,
    std::shared_ptr<const chord_tooling::ChordConfig> chordConfig,
    const IsolateOptions &options)
{
    auto agentStore = chordConfig->getAgentStore();
    auto agentEntry = agentStore->getAgent(agentName);
    if (agentEntry == nullptr)
        return SandboxStatus::forCondition(SandboxCondition::kInvalidConfiguration,
            "unknown agent '{}'", agentName);
    auto agentEndpoint = agentEntry->agentLocation;

    auto securityConfig = chordConfig->getSecurityConfig();

    return connect(agentName, agentEndpoint, securityConfig, options);
}

tempo_utils::Result<std::shared_ptr<chord_sandbox::ChordIsolate>>
chord_sandbox::ChordIsolate::spawn(
    std::string_view agentName,
    std::shared_ptr<const chord_tooling::ChordConfig> chordConfig,
    const IsolateOptions &options) {
    auto agentStore = chordConfig->getAgentStore();
    auto agentEntry = agentStore->getAgent(agentName);
    if (agentEntry == nullptr)
        return SandboxStatus::forCondition(SandboxCondition::kInvalidConfiguration,
            "unknown agent '{}'", agentName);

    auto securityConfig = chordConfig->getSecurityConfig();

    return spawn(agentName, securityConfig, agentEntry, options);
}

tempo_utils::Result<std::shared_ptr<chord_sandbox::ChordIsolate>>
chord_sandbox::ChordIsolate::spawn(
    std::string_view agentName,
    std::shared_ptr<const chord_tooling::SecurityConfig> securityConfig,
    std::shared_ptr<const chord_tooling::AgentEntry> agentEntry,
    const IsolateOptions &options)
{
    auto &pemRootCABundleFile = securityConfig->pemRootCABundleFile;

    std::shared_ptr<AbstractEndpointSigner> endpointSigner;
    if (options.endpointSigner == nullptr) {
        tempo_security::CertificateKeyPair localCAKeypair;
        TU_ASSIGN_OR_RETURN (localCAKeypair, securityConfig->getSigningKeypair());
        endpointSigner = std::make_shared<LocalEndpointSigner>(localCAKeypair);
    } else {
        endpointSigner = options.endpointSigner;
    }

    internal::SpawnAgentResult spawnAgentResult;
    TU_ASSIGN_OR_RETURN (spawnAgentResult, internal::spawn_agent(
        options.agentPath,
        options.agentServerNameOverride,
        options.runDirectory,
        agentEntry->idleTimeout,
        agentEntry->pemCertificateFile,
        agentEntry->pemPrivateKeyFile,
        pemRootCABundleFile));

    // construct the channel credentials
    grpc::SslCredentialsOptions sslCredentialsOptions;
    tempo_utils::FileReader rootCABundleReader(pemRootCABundleFile);
    if (!rootCABundleReader.isValid())
        return SandboxStatus::forCondition(SandboxCondition::kInvalidConfiguration,
            "failed to read root CA bundle {}", pemRootCABundleFile.string());
    auto rootCABytes = rootCABundleReader.getBytes();
    sslCredentialsOptions.pem_root_certs = std::string((const char *) rootCABytes->getData(), rootCABytes->getSize());
    auto credentials = grpc::SslCredentials(sslCredentialsOptions);

    // construct the client
    grpc::ChannelArguments channelArguments;
    if (!options.agentServerNameOverride.empty()) {
        channelArguments.SetSslTargetNameOverride(options.agentServerNameOverride);
    }

    auto channel = grpc::CreateCustomChannel(spawnAgentResult.endpoint, credentials, channelArguments);
    auto priv = std::make_unique<SandboxPriv>();
    priv->stub = chord_invoke::InvokeService::NewStub(channel);

    // verify that the agent is running
    grpc::ClientContext context;
    chord_invoke::IdentifyAgentRequest identifyRequest;
    chord_invoke::IdentifyAgentResult identifyResult;
    auto status = priv->stub->IdentifyAgent(&context, identifyRequest, &identifyResult);
    if (!status.ok())
        return SandboxStatus::forCondition(SandboxCondition::kAgentError,
            "IdentifyAgent failed: {}", status.error_message());

    TU_LOG_INFO << "connected to agent " << identifyResult.agent_name();

    auto isolate = std::shared_ptr<ChordIsolate>(new ChordIsolate(
        agentName, pemRootCABundleFile, endpointSigner, channel, std::move(priv)));
    return isolate;
}

chord_sandbox::ChordIsolate::ChordIsolate(
    std::string_view agentName,
    const std::filesystem::path &pemRootCABundleFile,
    std::shared_ptr<AbstractEndpointSigner> endpointSigner,
    std::shared_ptr<grpc::Channel> channel,
    std::unique_ptr<SandboxPriv> priv)
    : m_name(agentName),
      m_pemRootCABundleFile(pemRootCABundleFile),
      m_endpointSigner(endpointSigner),
      m_channel(channel),
      m_priv(std::move(priv))
{
    TU_ASSERT (!m_name.empty());
    TU_ASSERT (!m_pemRootCABundleFile.empty());
    TU_ASSERT (m_endpointSigner != nullptr);
    TU_ASSERT (m_channel != nullptr);
    TU_ASSERT (m_priv != nullptr);
}

tempo_utils::Result<std::shared_ptr<chord_sandbox::RemoteMachine>>
chord_sandbox::ChordIsolate::launch(
    std::string_view name,
    const tempo_utils::Url &mainLocation,
    const tempo_config::ConfigMap &configMap,
    const std::vector<RequestedPortAndHandler> &plugs,
    bool startSuspended)
{
    TU_ASSERT (!name.empty());
    TU_ASSERT (mainLocation.isValid());

    absl::flat_hash_set<chord_common::RequestedPort> requestedPortsSet;
    absl::flat_hash_map<tempo_utils::Url,std::shared_ptr<chord_common::AbstractProtocolHandler>> plugHandlersMap;

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
        createMachineResult.machineUrl, createMachineResult.protocolEndpoints,
        createMachineResult.endpointCsrs, m_endpointSigner, absl::Hours(4)));

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
            m_pemRootCABundleFile, nameOverride));
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
        m_pemRootCABundleFile, nameOverride));

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
