
#include <chord_mesh/stream_acceptor.h>
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream.h>

chord_mesh::StreamAcceptor::StreamAcceptor(StreamHandle *handle)
    : m_handle(handle),
      m_state(AcceptorState::Initial)
{
    TU_ASSERT (m_handle != nullptr);
    m_handle->data = this;
}

chord_mesh::StreamAcceptor::~StreamAcceptor()
{
    shutdown();
}

tempo_utils::Result<std::shared_ptr<chord_mesh::StreamAcceptor>>
chord_mesh::StreamAcceptor::forUnix(
     std::string_view pipePath,
     int pipeFlags,
     StreamManager *manager)
{
    auto *loop = manager->getLoop();
    int ret;

    auto *pipe = (uv_pipe_t *) std::malloc(sizeof(uv_pipe_t));
    memset(pipe, 0, sizeof(uv_pipe_t));

    ret = uv_pipe_init(loop, pipe, false);
    if (ret != 0) {
        std::free(pipe);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_pipe_init failed: {}", uv_strerror(ret));
    }

    ret = uv_pipe_bind2(pipe, pipePath.data(), pipePath.size(), pipeFlags);
    if (ret != 0) {
        std::free(pipe);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_pipe_bind2 failed: {}", uv_strerror(ret));
    }

    auto *stream = (uv_stream_t *) pipe;
    auto *handle = manager->allocateHandle(stream);
    auto listener = std::shared_ptr<StreamAcceptor>(new StreamAcceptor(handle));
    handle->data = listener.get();

    return listener;
}

tempo_utils::Result<std::shared_ptr<chord_mesh::StreamAcceptor>>
chord_mesh::StreamAcceptor::forTcp4(
     std::string_view ipAddress,
     tu_uint16 tcpPort,
     StreamManager *manager)
{
    auto *loop = manager->getLoop();
    int ret;

    std::string addressString(ipAddress);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(sockaddr_in));
    ret = uv_ip4_addr(addressString.c_str(), tcpPort, &addr);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid ipv4 address: {}", uv_strerror(ret));

    auto *tcp = (uv_tcp_t *) std::malloc(sizeof(uv_tcp_t));
    memset(tcp, 0, sizeof(uv_tcp_t));

    ret = uv_tcp_init(loop, tcp);
    if (ret != 0) {
        std::free(tcp);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_tcp_init failed: {}", uv_strerror(ret));
    }

    ret = uv_tcp_bind(tcp, (const sockaddr *) &addr, 0);
    if (ret != 0) {
        std::free(tcp);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_tcp_bind failed: {}", uv_strerror(ret));
    }

    auto *stream = (uv_stream_t *) tcp;
    auto *handle = manager->allocateHandle(stream);
    auto listener = std::shared_ptr<StreamAcceptor>(new StreamAcceptor(handle));
    handle->data = listener.get();

    return listener;
}

tempo_utils::Result<std::shared_ptr<chord_mesh::StreamAcceptor>>
chord_mesh::StreamAcceptor::forLocation(
    const chord_common::TransportLocation &endpoint,
    StreamManager *manager)
{
    switch (endpoint.getType()) {
        case chord_common::TransportType::Unix:
            return forUnix(endpoint.getUnixPath().c_str(), 0, manager);
        default:
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "invalid acceptor endpoint");
    }
}

chord_mesh::AcceptorState
chord_mesh::StreamAcceptor::getAcceptorState() const
{
    return m_state;
}

void
chord_mesh::new_unix_listener(uv_stream_t *server, int err)
{
    auto *handle = (StreamHandle *) server->data;
    auto *acceptor = (StreamAcceptor *) handle->data;

    if (err < 0) {
        acceptor->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "failed to accept connection: {}", uv_strerror(err)));
        return;
    }

    int ret;

    auto *pipe = (uv_pipe_t *) std::malloc(sizeof(uv_pipe_t));
    memset(pipe, 0, sizeof(uv_pipe_t));

    ret = uv_pipe_init(server->loop, pipe, false);
    if (ret != 0) {
        std::free(pipe);
        acceptor->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_pipe_init error: {}", uv_strerror(err)));
        return;
    }

    auto *client = (uv_stream_t *) pipe;

    ret = uv_accept(server, client);
    if (ret != 0) {
        std::free(pipe);
        acceptor->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_accept error: {}", uv_strerror(err)));
        return;
    }

    auto *manager = handle->manager;
    auto &ops = acceptor->m_ops;
    auto &options = acceptor->m_options;

    auto stream = std::make_shared<Stream>(manager->allocateHandle(client), /* initiator= */ false, !options.allowInsecure);
    ops.accept(stream, acceptor->m_options.data);
}

void
chord_mesh::new_tcp4_listener(uv_stream_t *server, int err)
{
    auto *handle = (StreamHandle *) server->data;
    auto *acceptor = (StreamAcceptor *) handle->data;

    if (err < 0) {
        acceptor->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "failed to accept connection: {}", uv_strerror(err)));
        return;
    }

    int ret;

    auto *tcp = (uv_tcp_t *) std::malloc(sizeof(uv_tcp_t));
    memset(tcp, 0, sizeof(uv_tcp_t));

    ret = uv_tcp_init(server->loop, tcp);
    if (ret != 0) {
        std::free(tcp);
        acceptor->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_tcp_init error: {}", uv_strerror(err)));
        return;
    }

    auto *client = (uv_stream_t *) tcp;

    ret = uv_accept(server, client);
    if (ret != 0) {
        std::free(tcp);
        acceptor->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_accept error: {}", uv_strerror(err)));
        return;
    }

    auto *manager = handle->manager;
    auto &ops = acceptor->m_ops;
    auto &options = acceptor->m_options;

    auto stream = std::make_shared<Stream>(manager->allocateHandle(client), /* initiator= */ false, !options.allowInsecure);
    ops.accept(stream, acceptor->m_options.data);
}

tempo_utils::Status
chord_mesh::StreamAcceptor::listen(const StreamAcceptorOps &ops, const StreamAcceptorOptions &options)
{
    if (m_state != AcceptorState::Initial)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid acceptor state");

    int ret;

    switch (m_handle->stream->type) {
        case UV_NAMED_PIPE:
            ret = uv_listen(m_handle->stream, 64, new_unix_listener);
            break;
        case UV_TCP:
            ret = uv_listen(m_handle->stream, 64, new_tcp4_listener);
            break;
        default:
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "invalid handle type for stream acceptor");
    }

    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_listen error: {}", uv_strerror(ret));

    m_state = AcceptorState::Active;
    m_ops = ops;
    m_options = options;

    return {};
}

void
chord_mesh::StreamAcceptor::shutdown()
{
    switch (m_state) {
        case AcceptorState::Initial:
            m_handle->close();
            break;
        case AcceptorState::Active:
            m_handle->shutdown();
            break;
        default:
            return;
    }

    m_state = AcceptorState::Closed;

    if (m_ops.cleanup != nullptr) {
        m_ops.cleanup(m_options.data);
    }
}

void
chord_mesh::StreamAcceptor::emitError(const tempo_utils::Status &status)
{
    if (m_ops.error != nullptr) {
        m_ops.error(status, m_options.data);
    }
}
