
#include <grpcpp/create_channel.h>

#include <chord_sandbox/remoting_client.h>

chord_sandbox::RemotingClient::RemotingClient(
    const tempo_utils::Url &endpointUrl,
    const tempo_utils::Url &protocolUrl,
    std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler,
    std::shared_ptr<grpc::ChannelCredentials> credentials,
    const std::string &endpointServerName)
    : m_endpointUrl(endpointUrl),
      m_protocolUrl(protocolUrl),
      m_handler(handler),
      m_credentials(credentials),
      m_endpointServerName(endpointServerName)
{
    TU_ASSERT (m_endpointUrl.isValid());
    TU_ASSERT (m_protocolUrl.isValid());
    TU_ASSERT (m_handler != nullptr);
    TU_ASSERT (m_credentials != nullptr);
}

tempo_utils::Status
chord_sandbox::RemotingClient::connect()
{
    absl::MutexLock lock(&m_lock);

    if (m_stub)
        return SandboxStatus::forCondition(
            SandboxCondition::kSandboxInvariant, "remoting client is already connected");

    // construct the client
    grpc::ChannelArguments channelArguments;
    if (!m_endpointServerName.empty()) {
        TU_LOG_INFO << "using target name override " << m_endpointServerName << " for endpoint " << m_endpointUrl;
        channelArguments.SetSslTargetNameOverride(m_endpointServerName);
    }
    m_channel = grpc::CreateCustomChannel(m_endpointUrl.toString(), m_credentials, channelArguments);
    m_stub = chord_remoting::RemotingService::NewStub(m_channel);

    // start the communication stream
    new ClientCommunicationStream(m_stub.get(), m_protocolUrl, m_handler, true);
    TU_LOG_INFO << "starting communication with " << m_protocolUrl << " on endpoint " << m_endpointUrl;

    return SandboxStatus::ok();
}

tempo_utils::Status
chord_sandbox::RemotingClient::shutdown()
{
    absl::MutexLock lock(&m_lock);
    if (m_stub) {
        TU_LOG_INFO << "shutting down communication with " << m_protocolUrl;
        m_stub.reset();
    }
    return SandboxStatus::ok();
}

chord_sandbox::ClientCommunicationStream::ClientCommunicationStream(
    chord_remoting::RemotingService::StubInterface *stub,
    const tempo_utils::Url &protocolUrl,
    std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler,
    bool freeWhenDone)
    : m_protocolUrl(protocolUrl),
      m_handler(handler),
      m_freeWhenDone(freeWhenDone),
      m_head(nullptr),
      m_tail(nullptr)
{
    TU_ASSERT (stub != nullptr);
    TU_ASSERT (m_protocolUrl.isValid());
    TU_ASSERT (m_handler != nullptr);
    m_context.AddMetadata("x-zuri-protocol-url", m_protocolUrl.toString());
    auto *async = stub->async();
    async->Communicate(&m_context, this);
    m_handler->attach(this);
    StartCall();
}

chord_sandbox::ClientCommunicationStream::~ClientCommunicationStream()
{
    if (m_handler->isAttached()) {
        TU_LOG_WARN << "handler was still attached to ClientCommunicationStream during cleanup";
        m_handler->detach();
    }
    while (m_head != nullptr) {
        auto *curr = m_head;
        m_head = m_head->next;
        delete curr;
    }
}

void
chord_sandbox::ClientCommunicationStream::OnReadInitialMetadataDone(bool ok)
{
    TU_LOG_INFO_IF(!ok) << "failed to read initial metadata";
    StartRead(&m_incoming);
}

void
chord_sandbox::ClientCommunicationStream::OnReadDone(bool ok)
{
    if (!ok) {
        TU_LOG_VV << "read failed";
        return;
    }
    m_handler->handle(m_incoming.data());
    m_incoming.Clear();
    StartRead(&m_incoming);
}

void
chord_sandbox::ClientCommunicationStream::OnWriteDone(bool ok)
{
    if (!ok) {
        TU_LOG_VV << "write failed";
        return;
    }
    absl::MutexLock locker(&m_lock);
    auto *pending = m_head->next;
    delete m_head;
    m_head = pending;
    if (pending) {
        StartWrite(&pending->message);
    }
}

void
chord_sandbox::ClientCommunicationStream::OnDone(const grpc::Status &status)
{
    TU_LOG_INFO << "remote end closed with status "
        << status.error_message() << " (" << status.error_details() << ")";
    m_handler->detach();
    if (m_freeWhenDone) {
        delete this;
    }
}

tempo_utils::Status
chord_sandbox::ClientCommunicationStream::write(std::string_view message)
{
    auto *pending = new PendingWrite();
    pending->message.set_version(chord_remoting::MessageVersion::Version1);
    pending->message.set_data(std::string(message.data(), message.size()));
    pending->next = nullptr;

    absl::MutexLock locker(&m_lock);

    if (m_head == nullptr) {
        m_head = pending;
        m_tail = pending;
        // if no messages are pending then start the write
        StartWrite(&pending->message);
    } else {
        // otherwise just enqueue the pending message
        m_tail->next = pending;
        m_tail = pending;
    }

    return SandboxStatus::ok();
}