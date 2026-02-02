
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/noise.h>
#include <chord_mesh/stream_manager.h>
#include <chord_mesh/stream_session.h>

chord_mesh::StreamManager::StreamManager(
    uv_loop_t *loop,
    const tempo_security::CertificateKeyPair &keypair,
    std::shared_ptr<tempo_security::X509Store> trustStore,
    const StreamManagerOps &ops,
    const StreamManagerOptions &options)
    : m_loop(loop),
      m_keypair(keypair),
      m_trustStore(std::move(trustStore)),
      m_ops(ops),
      m_options(options),
      m_connects(nullptr),
      m_accepts(nullptr),
      m_streams(nullptr),
      m_running(true)
{
    TU_ASSERT (m_loop != nullptr);
    TU_ASSERT (m_keypair.isValid());
    TU_ASSERT (m_trustStore != nullptr);
}

uv_loop_t *
chord_mesh::StreamManager::getLoop() const
{
    return m_loop;
}

std::shared_ptr<tempo_security::X509Store>
chord_mesh::StreamManager::getTrustStore() const
{
    return m_trustStore;
}

tempo_security::CertificateKeyPair
chord_mesh::StreamManager::getKeypair() const
{
    return m_keypair;
}

std::string
chord_mesh::StreamManager::getProtocolName() const
{
    if (!m_options.protocolName.empty())
        return m_options.protocolName;
    return kDefaultNoiseProtocol;
}

chord_mesh::ConnectHandle *
chord_mesh::StreamManager::allocateConnectHandle(
    uv_connect_t *connect,
    bool insecure,
    std::unique_ptr<AbstractConnectContext> &&ctx)
{
    TU_ASSERT (connect != nullptr);

    if (!m_running)
        return nullptr;

    auto *handle = new ConnectHandle(connect, this, insecure, std::move(ctx));
    connect->data = handle;
    connect->handle->data = handle;
    handle->req = connect;
    handle->manager = this;

    // case 1: there are no active handles
    if (m_connects == nullptr) {
        m_connects = handle;
        handle->prev = nullptr;
        handle->next = nullptr;
        return handle;
    }

    auto *next = m_connects->next;
    handle->next = m_connects;

    // case 2: there is a single active handle
    if (next == nullptr) {
        TU_ASSERT (m_connects->prev == nullptr);
        handle->prev = m_connects;
        m_connects->prev = handle;
        m_connects->next = handle;
        return handle;
    }

    // case 3: there is more than 1 active handle
    auto *prev = m_connects->prev;
    handle->prev = prev;
    next->prev = handle;
    prev->next = handle;
    return handle;
}

chord_mesh::AcceptHandle *
chord_mesh::StreamManager::allocateAcceptHandle(
    uv_stream_t *accept,
    bool insecure,
    std::unique_ptr<AbstractAcceptContext> &&ctx)
{
    TU_ASSERT (accept != nullptr);

    if (!m_running)
        return nullptr;

    auto *handle = new AcceptHandle(accept, this, insecure, std::move(ctx));
    accept->data = handle;
    handle->stream = accept;
    handle->manager = this;

    // case 1: there are no active handles
    if (m_accepts == nullptr) {
        m_accepts = handle;
        handle->prev = nullptr;
        handle->next = nullptr;
        return handle;
    }

    auto *next = m_accepts->next;
    handle->next = m_accepts;

    // case 2: there is a single active handle
    if (next == nullptr) {
        TU_ASSERT (m_accepts->prev == nullptr);
        handle->prev = m_accepts;
        m_accepts->prev = handle;
        m_accepts->next = handle;
        return handle;
    }

    // case 3: there is more than 1 active handle
    auto *prev = m_accepts->prev;
    handle->prev = prev;
    next->prev = handle;
    prev->next = handle;
    return handle;
}

chord_mesh::StreamHandle *
chord_mesh::StreamManager::allocateStreamHandle(uv_stream_t *stream, bool initiator, bool insecure)
{
    TU_ASSERT (stream != nullptr);

    if (!m_running)
        return nullptr;

    auto *handle = new StreamHandle(stream, this, initiator, insecure);
    stream->data = handle;
    handle->stream = stream;
    handle->manager = this;

    // case 1: there are no active handles
    if (m_streams == nullptr) {
        m_streams = handle;
        handle->prev = nullptr;
        handle->next = nullptr;
        return handle;
    }

    auto *next = m_streams->next;
    handle->next = m_streams;

    // case 2: there is a single active handle
    if (next == nullptr) {
        TU_ASSERT (m_streams->prev == nullptr);
        handle->prev = m_streams;
        m_streams->prev = handle;
        m_streams->next = handle;
        return handle;
    }

    // case 3: there is more than 1 active handle
    auto *prev = m_streams->prev;
    handle->prev = prev;
    next->prev = handle;
    prev->next = handle;
    return handle;
}

void
chord_mesh::StreamManager::freeConnectHandle(ConnectHandle *handle)
{
    TU_ASSERT (handle != nullptr);
    handle->ctx->cleanup();

    // case 1: the handle has no siblings
    if (handle->next == nullptr) {
        TU_ASSERT (handle->prev == nullptr);
        TU_ASSERT (m_connects == handle);
        delete handle;
        m_connects = nullptr;
        return;
    }

    TU_ASSERT (handle->prev != nullptr);
    auto *prev = handle->prev;
    auto *next = handle->next;

    // case 2: the handle has one sibling
    if (prev == next) {
        if (m_connects == handle) {
            m_connects = next;
        }
        m_connects->prev = nullptr;
        m_connects->next = nullptr;
        delete handle;
        return;
    }

    // case 3: the handle has more than sibling
    if (m_connects == handle) {
        m_connects = next;
    }
    prev->next = next;
    next->prev = prev;
    delete handle;
}

void
chord_mesh::StreamManager::freeAcceptHandle(AcceptHandle *handle)
{
    TU_ASSERT (handle != nullptr);

    // case 1: the handle has no siblings
    if (handle->next == nullptr) {
        TU_ASSERT (handle->prev == nullptr);
        TU_ASSERT (m_accepts == handle);
        delete handle;
        m_accepts = nullptr;
        return;
    }

    TU_ASSERT (handle->prev != nullptr);
    auto *prev = handle->prev;
    auto *next = handle->next;

    // case 2: the handle has one sibling
    if (prev == next) {
        if (m_accepts == handle) {
            m_accepts = next;
        }
        m_accepts->prev = nullptr;
        m_accepts->next = nullptr;
        delete handle;
        return;
    }

    // case 3: the handle has more than sibling
    if (m_accepts == handle) {
        m_accepts = next;
    }
    prev->next = next;
    next->prev = prev;
    delete handle;
}

void
chord_mesh::StreamManager::freeStreamHandle(StreamHandle *handle)
{
    TU_ASSERT (handle != nullptr);

    // case 1: the handle has no siblings
    if (handle->next == nullptr) {
        TU_ASSERT (handle->prev == nullptr);
        TU_ASSERT (m_streams == handle);
        delete handle;
        m_streams = nullptr;
        return;
    }

    TU_ASSERT (handle->prev != nullptr);
    auto *prev = handle->prev;
    auto *next = handle->next;

    // case 2: the handle has one sibling
    if (prev == next) {
        if (m_streams == handle) {
            m_streams = next;
        }
        m_streams->prev = nullptr;
        m_streams->next = nullptr;
        delete handle;
        return;
    }

    // case 3: the handle has more than sibling
    if (m_streams == handle) {
        m_streams = next;
    }
    prev->next = next;
    next->prev = prev;
    delete handle;
}

void
chord_mesh::StreamManager::shutdown()
{
    if (!m_running)
        return;
    m_running = false;
}

void
chord_mesh::StreamManager::emitError(const tempo_utils::Status &status)
{
    if (m_ops.error != nullptr) {
        m_ops.error(status, m_options.data);
    }
}

chord_mesh::ConnectHandle::ConnectHandle(
    uv_connect_t *req,
    StreamManager *manager,
    bool insecure,
    std::unique_ptr<AbstractConnectContext> &&ctx)
    : req(req),
      manager(manager),
      insecure(insecure),
      ctx(std::move(ctx)),
      id(tempo_utils::UUID::randomUUID()),
      state(ConnectState::Pending),
      prev(nullptr),
      next(nullptr),
      m_shared(true)
{
    TU_ASSERT (this->req != nullptr);
    TU_ASSERT (this->manager != nullptr);
    TU_ASSERT (this->ctx != nullptr);
}

chord_mesh::ConnectHandle::~ConnectHandle()
{
    if (req != nullptr) {
        if (req->handle != nullptr) {
            std::free(req->handle);
        }
        std::free(req);
    }
}

void
chord_mesh::close_connect(uv_handle_t *connect)
{
    auto *handle = (ConnectHandle *) connect->data;
    if (!handle->m_shared) {
        auto *manager = handle->manager;
        manager->freeConnectHandle(handle);
    }
}

void
chord_mesh::ConnectHandle::connect(std::shared_ptr<Stream> stream)
{
    TU_ASSERT (state == ConnectState::Pending);
    TU_ASSERT (ctx != nullptr);
    ctx->connect(stream);
    state = ConnectState::Complete;
    if (!m_shared) {
        manager->freeConnectHandle(this);
    }
}

void
chord_mesh::ConnectHandle::error(const tempo_utils::Status &status)
{
    TU_ASSERT (state == ConnectState::Pending);
    TU_ASSERT (ctx != nullptr);
    ctx->error(status);
    state = ConnectState::Failed;
    if (!m_shared) {
        manager->freeConnectHandle(this);
    }
}

void
chord_mesh::ConnectHandle::abort()
{
    if (state == ConnectState::Pending) {
        state = ConnectState::Aborted;
        uv_close((uv_handle_t *) req->handle, close_connect);
    }
}

void
chord_mesh::ConnectHandle::release()
{
    m_shared = false;
    if (state != ConnectState::Pending) {
        manager->freeConnectHandle(this);
    }
}

chord_mesh::AcceptHandle::AcceptHandle(
    uv_stream_t *stream,
    StreamManager *manager,
    bool insecure,
    std::unique_ptr<AbstractAcceptContext> &&ctx)
    : stream(stream),
      manager(manager),
      insecure(insecure),
      ctx(std::move(ctx)),
      id(tempo_utils::UUID::randomUUID()),
      state(AcceptState::Active),
      prev(nullptr),
      next(nullptr),
      m_shared(true),
      m_req{}
{
    TU_ASSERT (this->stream != nullptr);
    TU_ASSERT (this->manager != nullptr);
    TU_ASSERT (this->ctx != nullptr);
}

chord_mesh::AcceptHandle::~AcceptHandle()
{
    if (stream != nullptr) {
        std::free(stream);
    }
}

void
chord_mesh::AcceptHandle::accept(std::shared_ptr<Stream> initiator)
{
    TU_ASSERT (state == AcceptState::Active);
    TU_ASSERT (ctx != nullptr);
    ctx->accept(std::move(initiator));
}

void
chord_mesh::AcceptHandle::error(const tempo_utils::Status &status)
{
    TU_ASSERT (ctx != nullptr);
    ctx->error(status);
}

void
chord_mesh::close_accept(uv_handle_t *stream)
{
    auto *handle = (AcceptHandle *) stream->data;

    handle->state = AcceptState::Closed;
    handle->ctx->cleanup();

    if (!handle->m_shared) {
        auto *manager = handle->manager;
        manager->freeAcceptHandle(handle);
    }
}

void
chord_mesh::shutdown_accept(uv_shutdown_t *req, int err)
{
    auto *handle = (AcceptHandle *) req->handle->data;
    auto *manager = handle->manager;
    if (err < 0) {
        manager->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_shutdown error: {}", uv_strerror(err)));
    }
    uv_close((uv_handle_t *) req->handle, close_accept);
    handle->state = AcceptState::Closing;
}

void
chord_mesh::AcceptHandle::shutdown()
{
    if (state != AcceptState::Active)
        return;
    memset(&m_req, 0, sizeof(uv_shutdown_t));
    auto ret = uv_shutdown(&m_req, stream, shutdown_accept);
    if (ret < 0) {
        manager->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_shutdown error: {}", uv_strerror(ret)));
        close();
    } else {
        state = AcceptState::ShuttingDown;
    }
}

void
chord_mesh::AcceptHandle::close()
{
    if (state != AcceptState::Active)
        return;
    uv_close((uv_handle_t *) stream, close_accept);
    state = AcceptState::Closing;
}

void
chord_mesh::AcceptHandle::release()
{
    m_shared = false;
    if (state == AcceptState::Closed) {
        manager->freeAcceptHandle(this);
    }
}

chord_mesh::StreamHandle::StreamHandle(
    uv_stream_t *stream,
    StreamManager *manager,
    bool initiator,
    bool insecure)
    : stream(stream),
      manager(manager),
      initiator(initiator),
      insecure(insecure),
      id(tempo_utils::UUID::randomUUID()),
      state(StreamState::Initial),
      prev(nullptr),
      next(nullptr),
      m_shared(true),
      m_req{}
{
    TU_ASSERT (this->stream != nullptr);
    TU_ASSERT (this->manager != nullptr);
    session = std::make_unique<StreamSession>(this, initiator, insecure);
}

chord_mesh::StreamHandle::~StreamHandle()
{
    if (stream != nullptr) {
        std::free(stream);
    }
}

tempo_utils::Status
chord_mesh::StreamHandle::start(std::unique_ptr<AbstractStreamContext> &&ctx_)
{
    TU_ASSERT (ctx_ != nullptr);

    this->ctx = std::move(ctx_);
    state = StreamState::Active;
    return session->start();
}

tempo_utils::Status
chord_mesh::StreamHandle::negotiate(std::string_view protocolName)
{
    return session->negotiate(protocolName);
}

tempo_utils::Status
chord_mesh::StreamHandle::validate(
    std::string_view protocolName,
    std::shared_ptr<tempo_security::X509Certificate> certificate)
{
    TU_ASSERT (ctx != nullptr);
    return ctx->validate(protocolName, std::move(certificate));
}

tempo_utils::Status
chord_mesh::StreamHandle::send(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes)
{
    return session->write(std::move(bytes));
}

void
chord_mesh::StreamHandle::receive(const Envelope &envelope)
{
    TU_ASSERT (ctx != nullptr);
    ctx->receive(envelope);
}

void
chord_mesh::StreamHandle::error(const tempo_utils::Status &status)
{
    TU_ASSERT (ctx != nullptr);
    ctx->error(status);
}

void
chord_mesh::close_stream(uv_handle_t *stream)
{
    auto *handle = (StreamHandle *) stream->data;

    handle->state = StreamState::Closed;
    if (handle->ctx != nullptr) {
        handle->ctx->cleanup();
    }

    if (!handle->m_shared) {
        auto *manager = handle->manager;
        manager->freeStreamHandle(handle);
    }
}

void
chord_mesh::shutdown_stream(uv_shutdown_t *req, int err)
{
    auto *handle = (StreamHandle *) req->handle->data;
    auto *manager = handle->manager;
    if (err < 0) {
        manager->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_shutdown error: {}", uv_strerror(err)));
    }
    uv_close((uv_handle_t *) req->handle, close_stream);
    handle->state = StreamState::Closing;
}

void
chord_mesh::StreamHandle::shutdown()
{
    switch (state) {
        case StreamState::Initial:
        case StreamState::Active:
            break;
        default:
            return;
    }
    memset(&m_req, 0, sizeof(uv_shutdown_t));
    auto ret = uv_shutdown(&m_req, stream, shutdown_stream);
    if (ret < 0) {
        manager->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_shutdown error: {}", uv_strerror(ret)));
        close();
    } else {
        state = StreamState::ShuttingDown;
    }
}

void
chord_mesh::StreamHandle::close()
{
    switch (state) {
        case StreamState::Initial:
        case StreamState::Active:
            break;
        default:
            return;
    }
    uv_close((uv_handle_t *) stream, close_stream);
    state = StreamState::Closing;
}

void
chord_mesh::StreamHandle::release()
{
    m_shared = false;
    if (state == StreamState::Closed) {
        manager->freeStreamHandle(this);
    }
}
