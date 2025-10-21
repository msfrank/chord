#include <sys/wait.h>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/ascii.h>
#include <grpcpp/create_channel.h>

#include <chord_invoke/invoke_service.grpc.pb.h>
#include <chord_sandbox/internal/machine_utils.h>
#include <chord_sandbox/internal/session_utils.h>
#include <chord_sandbox/local_certificate_signer.h>
#include <chord_sandbox/run_protocol_plug.h>
#include <tempo_security/x509_certificate_signing_request.h>
#include <tempo_utils/directory_maker.h>
#include <tempo_utils/file_reader.h>
#include <tempo_utils/log_stream.h>
#include <tempo_utils/url.h>

struct chord_sandbox::ChordIsolate::SandboxPriv {
    std::unique_ptr<chord_invoke::InvokeService::Stub> stub;
};

#include <chord_sandbox/chord_isolate.h>
#include <chord_sandbox/grpc_connector.h>

tempo_utils::Result<std::shared_ptr<chord_sandbox::ChordIsolate>>
chord_sandbox::ChordIsolate::connect(
    const std::filesystem::path &sessionDirectory,
    const std::filesystem::path &pemRootCABundleFile,
    std::shared_ptr<chord_common::AbstractCertificateSigner> certificateSigner,
    absl::Duration connectTimeout)
{
    chord_common::TransportLocation endpoint;
    TU_ASSIGN_OR_RETURN (endpoint, internal::load_session_endpoint(sessionDirectory, connectTimeout));

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
    channelArguments.SetSslTargetNameOverride(endpoint.getServerName());

    auto target = endpoint.toGrpcTarget();
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

    auto sessionName = sessionDirectory.filename().string();
    auto isolate = std::shared_ptr<ChordIsolate>(new ChordIsolate(
        sessionName, pemRootCABundleFile, certificateSigner, channel, std::move(priv)));
    return isolate;
}

// tempo_utils::Result<std::shared_ptr<chord_sandbox::ChordIsolate>>
// chord_sandbox::ChordIsolate::spawn(
//     std::string_view agentName,
//     std::shared_ptr<const chord_tooling::ChordConfig> chordConfig,
//     const IsolateOptions &options) {
//     auto agentStore = chordConfig->getAgentStore();
//     auto agentEntry = agentStore->getAgent(agentName);
//     if (agentEntry == nullptr)
//         return SandboxStatus::forCondition(SandboxCondition::kInvalidConfiguration,
//             "unknown agent '{}'", agentName);
//
//     auto securityConfig = chordConfig->getSecurityConfig();
//
// std::shared_ptr<chord_common::AbstractCertificateSigner> certificateSigner;
// if (options.certificateSigner == nullptr) {
//     tempo_security::CertificateKeyPair localCAKeypair;
//     TU_ASSIGN_OR_RETURN (localCAKeypair, securityConfig->getSigningKeypair());
//     certificateSigner = std::make_shared<LocalCertificateSigner>(localCAKeypair);
// } else {
//     certificateSigner = options.certificateSigner;
// }
//
//     return spawn(agentName, securityConfig, agentEntry, options);
// }

tempo_utils::Result<std::shared_ptr<chord_sandbox::ChordIsolate>>
chord_sandbox::ChordIsolate::spawn(
    std::string_view sessionName,
    const std::filesystem::path &runDirectory,
    const std::filesystem::path &agentPath,
    const std::filesystem::path &pemRootCABundleFile,
    std::shared_ptr<chord_common::AbstractCertificateSigner> certificateSigner,
    absl::Duration idleTimeout,
    absl::Duration registrationTimeout)
{
    tempo_utils::DirectoryMaker dirmaker(runDirectory, sessionName,
        std::filesystem::perms::owner_all);
    TU_RETURN_IF_NOT_OK (dirmaker.getStatus());
    auto sessionDirectory = dirmaker.getAbsolutePath();

    auto spawnResult = spawn(sessionDirectory, agentPath, pemRootCABundleFile,
        certificateSigner, idleTimeout, registrationTimeout);
    if (spawnResult.isStatus()) {
        std::error_code ec;
        std::filesystem::remove_all(sessionDirectory, ec);
    }
    return spawnResult;
}

tempo_utils::Result<std::shared_ptr<chord_sandbox::ChordIsolate>>
chord_sandbox::ChordIsolate::spawn(
    const std::filesystem::path &sessionDirectory,
    const std::filesystem::path &agentPath,
    const std::filesystem::path &pemRootCABundleFile,
    std::shared_ptr<chord_common::AbstractCertificateSigner> certificateSigner,
    absl::Duration idleTimeout,
    absl::Duration registrationTimeout)
{
    internal::PrepareSessionResult prepareSessionResult;
    TU_ASSIGN_OR_RETURN (prepareSessionResult, internal::prepare_session(
        sessionDirectory, pemRootCABundleFile, certificateSigner));

    internal::SpawnSessionResult spawnSessionResult;
    TU_ASSIGN_OR_RETURN (spawnSessionResult, internal::spawn_session(
        sessionDirectory, agentPath, pemRootCABundleFile,
        prepareSessionResult.pemCertificateFile, prepareSessionResult.pemPrivateKeyFile,
        idleTimeout, registrationTimeout));

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
    channelArguments.SetSslTargetNameOverride(spawnSessionResult.endpoint.getServerName());
    TU_LOG_INFO << "using target name " << spawnSessionResult.endpoint.getServerName();

    auto target = spawnSessionResult.endpoint.toGrpcTarget();
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

    auto sessionName = sessionDirectory.filename().string();
    auto isolate = std::shared_ptr<ChordIsolate>(new ChordIsolate(
        sessionName, pemRootCABundleFile, certificateSigner, channel, std::move(priv)));
    return isolate;
}

chord_sandbox::ChordIsolate::ChordIsolate(
    std::string_view agentName,
    const std::filesystem::path &pemRootCABundleFile,
    std::shared_ptr<chord_common::AbstractCertificateSigner> certificateSigner,
    std::shared_ptr<grpc::Channel> channel,
    std::unique_ptr<SandboxPriv> priv)
    : m_name(agentName),
      m_pemRootCABundleFile(pemRootCABundleFile),
      m_certificateSigner(certificateSigner),
      m_channel(channel),
      m_priv(std::move(priv))
{
    TU_ASSERT (!m_name.empty());
    TU_ASSERT (!m_pemRootCABundleFile.empty());
    TU_ASSERT (m_certificateSigner != nullptr);
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
        createMachineResult.endpointCsrs, m_certificateSigner, absl::Hours(4)));

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
