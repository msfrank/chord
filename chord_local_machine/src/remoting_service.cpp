
#include <chord_local_machine/remoting_service.h>
#include <tempo_utils/log_stream.h>
#include <tempo_utils/url.h>

RemotingService::RemotingService()
    : m_initComplete(nullptr)
{
}

RemotingService::RemotingService(uv_async_t *initComplete)
    : m_initComplete(initComplete)
{
    TU_ASSERT (initComplete != nullptr);
}

grpc::ServerBidiReactor<
    chord_remoting::Message,
    chord_remoting::Message> *
RemotingService::Communicate(grpc::CallbackServerContext *context)
{
    auto *stream = new CommunicationStream();

    // get the peer identity
    tempo_utils::Url protocolUrl;
    auto metadata = context->client_metadata();
    for (auto iterator = metadata.find("x-zuri-protocol-url"); iterator != metadata.cend(); iterator++) {
        if (protocolUrl.isValid()) {
            stream->Finish(
                grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "request must have a single protocol"));
            return stream;
        }
        auto value = iterator->second;
        protocolUrl = tempo_utils::Url::fromString(std::string_view (value.data(), value.size()));
        if (!protocolUrl.isValid()) {
            stream->Finish(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "invalid protocol"));
            return stream;
        }
    }

    TU_LOG_INFO << "peer identity is " << protocolUrl;

    {
        absl::MutexLock locker(&m_lock);

        // verify that handler exists for the specified protocol
        if (!m_handlers.contains(protocolUrl)) {
            stream->Finish(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                absl::StrCat("protocol ", protocolUrl.toString(), " is not allocated")));
            return stream;
        }

        // verify that handler is not attached
        if (m_attached.contains(protocolUrl)) {
            stream->Finish(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                absl::StrCat("protocol ", protocolUrl.toString(), " is already attached")));
            return stream;
        }

        // verify internal handler state
        auto handler = m_handlers.at(protocolUrl);
        if (handler->isAttached()) {
            stream->Finish(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                absl::StrCat("protocol ", protocolUrl.toString(), " is already attached")));
            return stream;
        }

        m_attached.insert(protocolUrl);
        auto status = stream->attachHandler(handler);
        if (status.notOk()) {
            stream->Finish(grpc::Status(grpc::StatusCode::INTERNAL,
                absl::StrCat("failed to attach protocol ", protocolUrl.toString())));
            return stream;
        }

        // if all required protocols are attached then signal init complete
        m_requiredAtLaunch.erase(protocolUrl);
        if (m_requiredAtLaunch.empty()) {
            uv_async_send(m_initComplete);
        }
    }

    return stream;
}

tempo_utils::Status
RemotingService::registerProtocolHandler(
    const tempo_utils::Url &protocolUrl,
    std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler,
    bool requiredAtLaunch)
{
    absl::MutexLock locker(&m_lock);

    if (m_handlers.contains(protocolUrl))
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation,
            "handler already exists for {}", protocolUrl.toString());

    m_handlers[protocolUrl] = handler;
    if (requiredAtLaunch) {
        m_requiredAtLaunch.insert(protocolUrl);
    }

    return tempo_utils::GenericStatus::ok();
}

bool
RemotingService::hasProtocolHandler(const tempo_utils::Url &protocolUrl)
{
    absl::MutexLock locker(&m_lock);
    return m_handlers.contains(protocolUrl);
}

std::shared_ptr<chord_protocol::AbstractProtocolHandler>
RemotingService::getProtocolHandler(const tempo_utils::Url &protocolUrl)
{
    absl::MutexLock locker(&m_lock);

    if (!m_handlers.contains(protocolUrl))
        return {};
    return m_handlers.at(protocolUrl);
}

CommunicationStream::CommunicationStream()
    : m_head(nullptr),
      m_tail(nullptr)
{
    TU_LOG_V << "stream started";
    StartRead(&m_incoming);
}

CommunicationStream::~CommunicationStream()
{
    if (m_handler->isAttached()) {
        TU_LOG_WARN << "handler was still attached to CommunicationStream during cleanup";
        m_handler->detach();
    }
}

void
CommunicationStream::OnReadDone(bool ok)
{
    if (!ok) {
        TU_LOG_V << "read failed";
        return;
    }
    // we don't need to hold the lock because reads are received one at a time
    m_handler->handle(m_incoming.data());
    m_incoming.Clear();
    StartRead(&m_incoming);
}

void
CommunicationStream::OnWriteDone(bool ok)
{
    if (!ok) {
        TU_LOG_V << "write failed";
        return;
    }

    absl::MutexLock locker(&m_lock);
    auto *pending = m_head->next;
    delete m_head;
    m_head = pending;
    if (pending) {
        StartWrite(&pending->message);
        TU_LOG_V << "starting write " << (void *) pending
            << " (size is " << (int) pending->message.data().size() << ")";
    }
}

void
CommunicationStream::OnDone()
{
    TU_LOG_V << "stream done";
    m_handler->detach();
    delete this;
}

tempo_utils::Status
CommunicationStream::write(std::string_view message)
{
    auto *pending = new PendingWrite();
    pending->message.set_version(chord_remoting::MessageVersion::Version1);
    pending->message.set_data(std::string(message));
    pending->next = nullptr;

    absl::MutexLock locker(&m_lock);

    if (m_head == nullptr) {
        m_head = pending;
        m_tail = pending;
        // if no messages are pending then start the write
        StartWrite(&pending->message);
        TU_LOG_V << "starting write " << (void *) pending
            << " (size is " << (int) pending->message.data().size() << ")";
    } else {
        // otherwise just enqueue the pending message
        m_tail->next = pending;
        TU_LOG_V << "enqueuing write " << (void *) pending
            << " (size is " << (int) pending->message.data().size() << ")";
    }

    return tempo_utils::GenericStatus::ok();
}

tempo_utils::Status
CommunicationStream::attachHandler(std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler)
{
    TU_ASSERT (!m_handler);
    m_handler = handler;
    m_handler->attach(this);
    return tempo_utils::GenericStatus::ok();
}