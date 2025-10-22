
#include <chord_machine/remoting_service.h>
#include <tempo_utils/log_stream.h>
#include <tempo_utils/url.h>

chord_machine::RemotingService::RemotingService()
    : m_initComplete(nullptr)
{
}

chord_machine::RemotingService::RemotingService(
    bool startSuspended,
    std::shared_ptr<LocalMachine> localMachine,
    uv_async_t *initComplete)
    : m_localMachine(localMachine),
      m_initComplete(initComplete)
{
    TU_ASSERT (m_localMachine != nullptr);
    TU_ASSERT (m_initComplete != nullptr);
    m_cachedState = startSuspended? chord_remoting::Suspended : chord_remoting::Running;
}

grpc::ServerUnaryReactor *
chord_machine::RemotingService::SuspendMachine(
    grpc::CallbackServerContext *context,
    const chord_remoting::SuspendMachineRequest *request,
    chord_remoting::SuspendMachineResult *response)
{
    auto *reactor = context->DefaultReactor();

    auto status = m_localMachine->suspend();
    if (status.notOk()) {
        reactor->Finish(grpc::Status(grpc::StatusCode::INTERNAL, std::string(status.getMessage())));
    } else {
        reactor->Finish(grpc::Status::OK);
    }

    return reactor;
}

grpc::ServerUnaryReactor *
chord_machine::RemotingService::ResumeMachine(
    grpc::CallbackServerContext *context,
    const chord_remoting::ResumeMachineRequest *request,
    chord_remoting::ResumeMachineResult *response)
{
    auto *reactor = context->DefaultReactor();

    auto status = m_localMachine->resume();
    if (status.notOk()) {
        reactor->Finish(grpc::Status(grpc::StatusCode::INTERNAL, std::string(status.getMessage())));
    } else {
        reactor->Finish(grpc::Status::OK);
    }

    return reactor;
}

grpc::ServerUnaryReactor *
chord_machine::RemotingService::TerminateMachine(
    grpc::CallbackServerContext *context,
    const chord_remoting::TerminateMachineRequest *request,
    chord_remoting::TerminateMachineResult *response)
{
    auto *reactor = context->DefaultReactor();

    auto status = m_localMachine->terminate();
    if (status.notOk()) {
        reactor->Finish(grpc::Status(grpc::StatusCode::INTERNAL, std::string(status.getMessage())));
    } else {
        reactor->Finish(grpc::Status::OK);
    }

    return reactor;
}

class FinishedCommunicateStream
    : public grpc::ServerBidiReactor<
        chord_remoting::Message,
        chord_remoting::Message>
{
public:
    explicit FinishedCommunicateStream(const grpc::Status &status) { Finish(status); };
    void OnDone() override {};
};

grpc::ServerBidiReactor<
    chord_remoting::Message,
    chord_remoting::Message> *
chord_machine::RemotingService::Communicate(grpc::CallbackServerContext *context)
{
    // get the peer identity
    tempo_utils::Url protocolUrl;
    auto metadata = context->client_metadata();
    for (auto iterator = metadata.find("x-zuri-protocol-url"); iterator != metadata.cend(); iterator++) {
        if (protocolUrl.isValid()) {
            return new FinishedCommunicateStream(
                grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "request must have a single protocol"));
        }
        auto value = iterator->second;
        protocolUrl = tempo_utils::Url::fromString(std::string_view (value.data(), value.size()));
        if (!protocolUrl.isValid()) {
            return new FinishedCommunicateStream(
                grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "invalid protocol"));
        }
    }

    TU_LOG_INFO << "peer identity is " << protocolUrl;
    return allocateCommunicateStream(protocolUrl);
}

grpc::ServerWriteReactor<chord_remoting::MonitorEvent> *
chord_machine::RemotingService::Monitor(
    grpc::CallbackServerContext *context,
    const ::chord_remoting::MonitorRequest *request)
{
    return allocateMonitorStream();
}

tempo_utils::Status
chord_machine::RemotingService::registerProtocolHandler(
    const tempo_utils::Url &protocolUrl,
    std::shared_ptr<chord_common::AbstractProtocolHandler> handler,
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
chord_machine::RemotingService::hasProtocolHandler(const tempo_utils::Url &protocolUrl)
{
    absl::MutexLock locker(&m_lock);
    return m_handlers.contains(protocolUrl);
}

std::shared_ptr<chord_common::AbstractProtocolHandler>
chord_machine::RemotingService::getProtocolHandler(const tempo_utils::Url &protocolUrl)
{
    absl::MutexLock locker(&m_lock);

    if (!m_handlers.contains(protocolUrl))
        return {};
    return m_handlers.at(protocolUrl);
}

void
chord_machine::RemotingService::notifyMachineStateChanged(chord_remoting::MachineState currState)
{
    absl::MutexLock locker(&m_lock);
    m_cachedState = currState;
    for (auto *stream : m_monitorStreams) {
        stream->notifyMachineStateChanged(currState);
    }
}

void
chord_machine::RemotingService::notifyMachineExit(tempo_utils::StatusCode statusCode)
{
    absl::MutexLock locker(&m_lock);
    for (auto *stream : m_monitorStreams) {
        stream->notifyMachineExit(statusCode);
    }
}

chord_machine::CommunicateStream *
chord_machine::RemotingService::allocateCommunicateStream(const tempo_utils::Url &protocolUrl)
{
    absl::MutexLock locker(&m_lock);

    auto *stream = new CommunicateStream(protocolUrl, this);

    // verify that handler exists for the specified protocol
    if (!m_handlers.contains(protocolUrl)) {
        stream->Finish(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
            absl::StrCat("protocol ", protocolUrl.toString(), " is not allocated")));
        return stream;
    }

    // verify that handler is not attached
    if (m_communicateStreams.contains(protocolUrl)) {
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

    m_communicateStreams[protocolUrl] = stream;
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

    m_communicateStreams[protocolUrl] = stream;
    return stream;
}

void
chord_machine::RemotingService::freeCommunicateStream(const tempo_utils::Url &protocolUrl)
{
    TU_ASSERT (protocolUrl.isValid());
    absl::MutexLock locker(&m_lock);
    auto entry = m_communicateStreams.find(protocolUrl);
    if (entry == m_communicateStreams.cend())
        return;
    delete entry->second;
    m_communicateStreams.erase(entry);
}

chord_machine::MonitorStream *
chord_machine::RemotingService::allocateMonitorStream()
{
    absl::MutexLock locker(&m_lock);
    auto *stream = new MonitorStream(this, m_cachedState);
    m_monitorStreams.insert(stream);
    return stream;
}

void
chord_machine::RemotingService::freeMonitorStream(MonitorStream *stream)
{
    TU_ASSERT (stream != nullptr);
    absl::MutexLock locker(&m_lock);
    m_monitorStreams.erase(stream);
    delete stream;
}

chord_machine::CommunicateStream::CommunicateStream(const tempo_utils::Url &protocolUrl, RemotingService *remotingService)
    : m_protcolUrl(protocolUrl),
      m_remotingService(remotingService),
      m_head(nullptr),
      m_tail(nullptr)
{
    TU_ASSERT (m_protcolUrl.isValid());
    TU_ASSERT (m_remotingService != nullptr);
    TU_LOG_V << "Communicate stream started";
    StartRead(&m_incoming);
}

chord_machine::CommunicateStream::~CommunicateStream()
{
    if (m_handler->isAttached()) {
        TU_LOG_WARN << "handler was still attached to CommunicateStream during cleanup";
        m_handler->detach();
    }
    while (m_head != nullptr) {
        auto *curr = m_head;
        m_head = m_head->next;
        delete curr;
    }
}

void
chord_machine::CommunicateStream::OnReadDone(bool ok)
{
    if (!ok) {
        TU_LOG_V << "read failed";
        Finish(grpc::Status::OK);
        return;
    }
    // we don't need to hold the lock because reads are received one at a time
    m_handler->handle(m_incoming.data());
    m_incoming.Clear();
    StartRead(&m_incoming);
}

void
chord_machine::CommunicateStream::OnWriteDone(bool ok)
{
    if (!ok) {
        TU_LOG_V << "write failed";
        return;
    }

    absl::MutexLock locker(&m_lock);
    TU_LOG_V << "completed write for " << (void *) m_head;
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
chord_machine::CommunicateStream::OnCancel()
{
    TU_LOG_V << "Communicate stream cancelled";
    Finish(grpc::Status::OK);
}

void
chord_machine::CommunicateStream::OnDone()
{
    TU_LOG_V << "Communicate stream done";
    m_handler->detach();
    m_remotingService->freeCommunicateStream(m_protcolUrl);
}

tempo_utils::Status
chord_machine::CommunicateStream::write(std::string_view message)
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
        m_tail = pending;
        TU_LOG_V << "enqueuing write " << (void *) pending
            << " (size is " << (int) pending->message.data().size() << ")";
    }

    return {};
}

tempo_utils::Status
chord_machine::CommunicateStream::attachHandler(std::shared_ptr<chord_common::AbstractProtocolHandler> handler)
{
    TU_ASSERT (!m_handler);
    m_handler = handler;
    m_handler->attach(this);
    return tempo_utils::GenericStatus::ok();
}

chord_machine::MonitorStream::MonitorStream(RemotingService *remotingService, chord_remoting::MachineState currState)
    : m_remotingService(remotingService),
      m_head(nullptr),
      m_tail(nullptr)
{
    TU_ASSERT (m_remotingService != nullptr);
    TU_LOG_V << "Monitor stream started";
    notifyMachineStateChanged(currState);
}

chord_machine::MonitorStream::~MonitorStream()
{
    while (m_head != nullptr) {
        auto *curr = m_head;
        m_head = m_head->next;
        delete curr;
    }
}

void
chord_machine::MonitorStream::OnWriteDone(bool ok)
{
    if (!ok) {
        TU_LOG_V << "write failed";
        return;
    }

    absl::MutexLock locker(&m_lock);
    TU_LOG_V << "completed write for " << m_head->event.DebugString();
    auto *pending = m_head->next;
    delete m_head;
    m_head = pending;
    if (pending) {
        StartWrite(&pending->event);
        TU_LOG_V << "starting write for " << pending->event.DebugString();
    }
}

void
chord_machine::MonitorStream::OnCancel()
{
    TU_LOG_V << "Monitor stream cancelled";
    Finish(grpc::Status::OK);
}

void
chord_machine::MonitorStream::OnDone()
{
    TU_LOG_V << "Monitor stream done";
    m_remotingService->freeMonitorStream(this);
}

tempo_utils::Status
chord_machine::MonitorStream::enqueueWrite(chord_remoting::MonitorEvent &&event)
{
    auto *pending = new PendingWrite();
    pending->event = std::move(event);
    pending->next = nullptr;

    absl::MutexLock locker(&m_lock);

    if (m_head == nullptr) {
        m_head = pending;
        m_tail = pending;
        // if no messages are pending then start the write
        StartWrite(&pending->event);
        TU_LOG_V << "starting write for " << pending->event.DebugString();
    } else {
        // otherwise just enqueue the pending message
        m_tail->next = pending;
        m_tail = pending;
        TU_LOG_V << "enqueuing write for " << pending->event.DebugString();
    }

    return {};
}

tempo_utils::Status
chord_machine::MonitorStream::notifyMachineStateChanged(chord_remoting::MachineState currState)
{
    chord_remoting::MonitorEvent event;
    auto *stateChanged = event.mutable_state_changed();
    stateChanged->set_curr_state(currState);
    return enqueueWrite(std::move(event));
}

tempo_utils::Status
chord_machine::MonitorStream::notifyMachineExit(tempo_utils::StatusCode statusCode)
{
    chord_remoting::MonitorEvent event;
    auto *machineExit = event.mutable_machine_exit();
    machineExit->set_exit_status((tu_int32) statusCode);
    return enqueueWrite(std::move(event));
}