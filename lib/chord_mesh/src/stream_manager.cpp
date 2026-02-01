
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/noise.h>
#include <chord_mesh/stream_manager.h>

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

chord_mesh::StreamHandle *
chord_mesh::StreamManager::allocateStreamHandle(uv_stream_t *stream, void *data)
{
    TU_ASSERT (stream != nullptr);

    if (!m_running)
        return nullptr;

    auto *handle = new StreamHandle(stream, this, data);
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
      shared(false),
      prev(nullptr),
      next(nullptr)
{
    TU_ASSERT (req != nullptr);
    TU_ASSERT (manager != nullptr);
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
    if (!handle->shared) {
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
    if (!shared) {
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
    if (!shared) {
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

chord_mesh::StreamHandle::StreamHandle(uv_stream_t *stream, StreamManager *manager, void *data)
    : stream(stream),
      manager(manager),
      data(data),
      prev(nullptr),
      next(nullptr),
      m_closing(false)
{
    TU_ASSERT (stream != nullptr);
    TU_ASSERT (manager != nullptr);
}

void
chord_mesh::close_stream(uv_handle_t *stream)
{
    auto *handle = (StreamHandle *) stream->data;
    auto *manager = handle->manager;
    manager->freeStreamHandle(handle);
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
}

void
chord_mesh::StreamHandle::shutdown()
{
    if (m_closing)
        return;
    memset(&m_req, 0, sizeof(uv_shutdown_t));
    auto ret = uv_shutdown(&m_req, stream, shutdown_stream);
    if (ret < 0) {
        manager->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_shutdown error: {}", uv_strerror(ret)));
        return;
    }

    m_closing = true;
}

void
chord_mesh::StreamHandle::close()
{
    if (m_closing)
        return;
    uv_close((uv_handle_t *) stream, close_stream);
    m_closing = true;
}
