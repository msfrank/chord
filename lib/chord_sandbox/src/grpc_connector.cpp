
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
      m_machineMonitor(std::make_shared<MachineMonitor>()),
      m_connected(false),
      m_monitorStream(nullptr)
{
    TU_ASSERT (m_machineUrl.isValid());
}

std::shared_ptr<chord_sandbox::MachineMonitor>
chord_sandbox::GrpcConnector::getMonitor() const
{
    return m_machineMonitor;
}

tempo_utils::Status
chord_sandbox::GrpcConnector::registerProtocolHandler(
    const tempo_utils::Url &protocolUrl,
    std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler,
    const tempo_utils::Url &endpointUrl,
    const std::filesystem::path &pemRootCABundleFile,
    const std::string &endpointServerName)
{
    absl::MutexLock lock(&m_lock);

    if (m_clients.contains(protocolUrl))
        return SandboxStatus::forCondition(SandboxCondition::kSandboxInvariant,
            "handler is already registered for protocol {}", protocolUrl.toString());

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
chord_sandbox::GrpcConnector::connect(
    const tempo_utils::Url &controlUrl,
    const std::filesystem::path &pemRootCABundleFile,
    const std::string &endpointServerName)
{
    absl::MutexLock lock(&m_lock);

    if (m_connected)
        return SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "already connected to machine");

    // construct the channel credentials
    grpc::SslCredentialsOptions options;
    tempo_utils::FileReader rootCABundleReader(pemRootCABundleFile);
    if (!rootCABundleReader.isValid())
        return SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "failed to read root CA bundle");
    auto rootCABytes = rootCABundleReader.getBytes();
    options.pem_root_certs = std::string((const char *) rootCABytes->getData(), rootCABytes->getSize());
    auto credentials = grpc::SslCredentials(options);

    // construct the control client
    grpc::ChannelArguments channelArguments;
    if (!endpointServerName.empty()) {
        TU_LOG_INFO << "using target name override " << endpointServerName << " for controller " << controlUrl;
        channelArguments.SetSslTargetNameOverride(endpointServerName);
    }

    auto channel = grpc::CreateCustomChannel(controlUrl.toString(), credentials, channelArguments);
    m_stub = chord_remoting::RemotingService::NewStub(channel);

    // start machine monitor
    m_monitorStream = new ClientMonitorStream(m_stub.get(), m_machineMonitor, false);

    // connect plug clients
    for (auto &client : m_clients) {
        TU_LOG_INFO << "connecting protocol " << client.first;
        auto priv = client.second;
        TU_RETURN_IF_NOT_OK (priv->client->connect());
    }

    m_connected = true;

    return {};
}

tempo_utils::Status
chord_sandbox::GrpcConnector::resume()
{
    absl::MutexLock lock(&m_lock);

    if (!m_connected)
        return SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "not connected to machine");

    grpc::ClientContext ctx;
    chord_remoting::ResumeMachineRequest request;
    chord_remoting::ResumeMachineResult result;

    auto status = m_stub->ResumeMachine(&ctx, request, &result);
    if (!status.ok())
        return SandboxStatus::forCondition(SandboxCondition::kMachineError,
            "failed to resume machine: {}", status.error_message());

    return {};
}

tempo_utils::Status
chord_sandbox::GrpcConnector::suspend()
{
    absl::MutexLock lock(&m_lock);

    if (!m_connected)
        return SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "not connected to machine");

    grpc::ClientContext ctx;
    chord_remoting::SuspendMachineRequest request;
    chord_remoting::SuspendMachineResult result;

    auto status = m_stub->SuspendMachine(&ctx, request, &result);
    if (!status.ok())
        return SandboxStatus::forCondition(SandboxCondition::kMachineError,
            "failed to suspend machine: {}", status.error_message());

    return {};
}

tempo_utils::Status
chord_sandbox::GrpcConnector::terminate()
{
    absl::MutexLock lock(&m_lock);

    if (!m_connected)
        return SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "not connected to machine");

    grpc::ClientContext ctx;
    chord_remoting::TerminateMachineRequest request;
    chord_remoting::TerminateMachineResult result;

    auto status = m_stub->TerminateMachine(&ctx, request, &result);
    if (!status.ok())
        return SandboxStatus::forCondition(SandboxCondition::kMachineError,
            "failed to terminate machine: {}", status.error_message());

    return {};
}

chord_sandbox::MachineMonitor::MachineMonitor()
    : m_state(chord_remoting::MachineState::UnknownState),
      m_statusCode(tempo_utils::StatusCode::kUnknown)
{
}

chord_remoting::MachineState
chord_sandbox::MachineMonitor::getState()
{
    absl::MutexLock locker(&m_lock);
    return m_state;
}

chord_remoting::MachineState
chord_sandbox::MachineMonitor::waitForStateChange(chord_remoting::MachineState prevState, int timeoutMillis)
{
    m_lock.Lock();

    // if the caller supplied previous state equals the current state (the normal case) then wait
    // on the condition variable until signaled by setState().
    if (prevState == m_state) {
        if (timeoutMillis > 0) {
            m_cond.WaitWithTimeout(&m_lock, absl::Milliseconds(timeoutMillis));
        } else {
            m_cond.Wait(&m_lock);
        }
    }

    // we hold the lock here, either reacquired by Wait or we didn't release it
    auto state = m_state;
    m_lock.Unlock();

    return state;
}

void
chord_sandbox::MachineMonitor::setState(chord_remoting::MachineState state)
{
    m_lock.Lock();
    if (state != m_state) {
        m_state = state;
        m_cond.SignalAll();
    }
    m_lock.Unlock();
}

tempo_utils::StatusCode
chord_sandbox::MachineMonitor::getStatusCode()
{
    absl::MutexLock locker(&m_lock);
    return m_statusCode;
}

void
chord_sandbox::MachineMonitor::setStatusCode(tempo_utils::StatusCode statusCode)
{
    absl::MutexLock locker(&m_lock);
    m_statusCode = statusCode;
}

chord_sandbox::ClientMonitorStream::ClientMonitorStream(
    chord_remoting::RemotingService::StubInterface *stub,
    std::shared_ptr<MachineMonitor> machineMonitor,
    bool freeWhenDone)
    : m_machineMonitor(machineMonitor),
      m_freeWhenDone(freeWhenDone)
{
    TU_ASSERT (stub != nullptr);
    TU_ASSERT (m_machineMonitor != nullptr);
    auto *async = stub->async();
    chord_remoting::MonitorRequest request;
    async->Monitor(&m_context, &request, this);
    StartCall();
    TU_LOG_INFO << "Monitor starting";
}

chord_sandbox::ClientMonitorStream::~ClientMonitorStream()
{
}

void
chord_sandbox::ClientMonitorStream::OnReadInitialMetadataDone(bool ok)
{
    if (!ok) {
        TU_LOG_WARN << "Monitor failed to read initial metadata";
    } else {
        TU_LOG_INFO << "Monitor read initial metadata";
    }
    StartRead(&m_incoming);
}

inline tempo_utils::StatusCode
exit_status_to_status_code(tu_int32 exitStatus)
{
    auto statusCode = static_cast<tempo_utils::StatusCode>(exitStatus);
    switch (statusCode) {
        case tempo_utils::StatusCode::kOk:
        case tempo_utils::StatusCode::kCancelled:
        case tempo_utils::StatusCode::kInvalidArgument:
        case tempo_utils::StatusCode::kDeadlineExceeded:
        case tempo_utils::StatusCode::kNotFound:
        case tempo_utils::StatusCode::kAlreadyExists:
        case tempo_utils::StatusCode::kPermissionDenied:
        case tempo_utils::StatusCode::kUnauthenticated:
        case tempo_utils::StatusCode::kResourceExhausted:
        case tempo_utils::StatusCode::kFailedPrecondition:
        case tempo_utils::StatusCode::kAborted:
        case tempo_utils::StatusCode::kUnavailable:
        case tempo_utils::StatusCode::kOutOfRange:
        case tempo_utils::StatusCode::kUnimplemented:
        case tempo_utils::StatusCode::kInternal:
        case tempo_utils::StatusCode::kUnknown:
            return statusCode;
        default:
            return tempo_utils::StatusCode::kUnknown;
    }
}

void
chord_sandbox::ClientMonitorStream::OnReadDone(bool ok)
{
    if (!ok) {
        TU_LOG_WARN << "Monitor read failed";
        return;
    }

    TU_LOG_INFO << "Monitor received event: " << m_incoming.DebugString();

    switch (m_incoming.event_case()) {
        case chord_remoting::MonitorEvent::kStateChanged: {
            const auto &event = m_incoming.state_changed();
            m_machineMonitor->setState(event.curr_state());
            break;
        }
        case chord_remoting::MonitorEvent::kMachineExit: {
            const auto &event = m_incoming.machine_exit();
            auto statusCode = exit_status_to_status_code(event.exit_status());
            m_machineMonitor->setStatusCode(statusCode);
            break;
        }
        default:
            break;
    }

    m_incoming.Clear();
    StartRead(&m_incoming);
}

void
chord_sandbox::ClientMonitorStream::OnDone(const grpc::Status &status)
{
    TU_LOG_INFO << "Monitor remote end closed with status "
                << status.error_message() << " (" << status.error_details() << ")";
    if (m_freeWhenDone) {
        delete this;
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