
#include <chord_sandbox/grpc_connector.h>
#include <chord_sandbox/remoting_client.h>
#include <tempo_utils/file_reader.h>

namespace chord_sandbox {
    struct ClientPriv {
        std::unique_ptr<RemotingClient> client;
    };
}

chord_sandbox::GrpcConnector::GrpcConnector(
    const tempo_utils::Url &machineUrl,
    const lyric_common::RuntimePolicy &policy)
    : m_machineUrl(machineUrl),
      m_policy(policy),
      m_running(false)
{
    TU_ASSERT (m_machineUrl.isValid());
}

chord_sandbox::SandboxStatus
chord_sandbox::GrpcConnector::registerProtocolHandler(
    const tempo_utils::Url &protocolUrl,
    std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler,
    const tempo_utils::Url &endpointUrl,
    const std::filesystem::path &pemRootCABundleFile,
    const std::string &endpointServerName)
{
    absl::MutexLock lock(&m_lock);

    if (m_running)
        return SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "connector is already running");
    if (m_clients.contains(protocolUrl))
        return SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "handler is already registered for protocol");

    // construct the channel credentials
    grpc::SslCredentialsOptions options;
    tempo_utils::FileReader rootCABundleReader(pemRootCABundleFile);
    if (!rootCABundleReader.isValid())
        return SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "failed to read root CA bundle");
    auto rootCABytes = rootCABundleReader.getBytes();
    options.pem_root_certs = std::string((const char *) rootCABytes->getData(), rootCABytes->getSize());
    auto credentials = grpc::SslCredentials(options);

    auto priv = std::make_shared<ClientPriv>();
    priv->client = std::make_unique<RemotingClient>(endpointUrl, protocolUrl,
        handler, credentials, endpointServerName);
    m_clients[protocolUrl] = priv;
    return SandboxStatus::ok();
}

tempo_utils::Status
chord_sandbox::GrpcConnector::start()
{
    absl::MutexLock lock(&m_lock);

    if (m_running)
        return SandboxStatus::ok();

    for (auto iterator = m_clients.cbegin(); iterator != m_clients.cend(); iterator++) {
        auto priv = iterator->second;
        TU_LOG_INFO << "connecting protocol " << iterator->first;
        auto status = priv->client->connect();
        if (status.notOk())
            return status;
    }

    m_running = true;
    return SandboxStatus::ok();
}

void
chord_sandbox::GrpcConnector::stop()
{
    absl::MutexLock lock(&m_lock);

    if (m_running) {
        m_clients.clear();
        m_running = false;
    }
}

chord_sandbox::JwtCallCredentialsPlugin::JwtCallCredentialsPlugin(
    const std::string &pemCertificate,
    const google::protobuf::Message *message)
{
}

const char *
chord_sandbox::JwtCallCredentialsPlugin::GetType() const
{
    return "zuri-credential";
}

grpc::Status
chord_sandbox::JwtCallCredentialsPlugin::GetMetadata(
    grpc::string_ref serviceUrl,
    grpc::string_ref methodName,
    const grpc::AuthContext &channelAuthContext,
    std::multimap<grpc::string, grpc::string> *metadata)
{
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "unimplemented");
}

bool
chord_sandbox::JwtCallCredentialsPlugin::IsBlocking() const
{
    return false;
}

grpc::string
chord_sandbox::JwtCallCredentialsPlugin::DebugString()
{
    return "JwtCallCredentialsPlugin";
}