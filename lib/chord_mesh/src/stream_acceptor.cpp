
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream.h>
#include <chord_mesh/stream_acceptor.h>

chord_mesh::StreamAcceptor::StreamAcceptor(StreamManager *manager, const StreamAcceptorOptions &options, Private)
    : m_manager(manager),
      m_options(options),
      m_handle(nullptr)
{
    TU_ASSERT (m_manager != nullptr);
}

chord_mesh::StreamAcceptor::~StreamAcceptor()
{
    if (m_handle != nullptr) {
        m_handle->shutdown();
        m_handle->release();
    }
}

tempo_utils::Result<std::shared_ptr<chord_mesh::StreamAcceptor>>
chord_mesh::StreamAcceptor::create(
    StreamManager *manager,
    const StreamAcceptorOptions &options)
{
    auto acceptor = std::make_shared<StreamAcceptor>(manager, options, Private{});
    return acceptor;
}

chord_mesh::AcceptState
chord_mesh::StreamAcceptor::getAcceptState() const
{
    if (m_handle == nullptr)
        return AcceptState::Initial;
    return m_handle->state;
}

void
chord_mesh::new_unix_listener(uv_stream_t *server, int err)
{
    auto *handle = (AcceptHandle *) server->data;

    if (err < 0) {
        handle->error(
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
        handle->error(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_pipe_init error: {}", uv_strerror(err)));
        return;
    }

    auto *client = (uv_stream_t *) pipe;

    ret = uv_accept(server, client);
    if (ret != 0) {
        std::free(pipe);
        handle->error(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_accept error: {}", uv_strerror(err)));
        return;
    }

    auto *manager = handle->manager;
    auto stream = std::make_shared<Stream>(manager->allocateStreamHandle(client, /* initiator= */ false, handle->insecure));
    handle->accept(stream);
}

tempo_utils::Status
chord_mesh::StreamAcceptor::listenUnix(
     std::string_view pipePath,
     int pipeFlags,
     std::unique_ptr<AbstractAcceptContext> &&ctx)
{
    if (m_handle != nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "acceptor is already listening");

    auto *loop = m_manager->getLoop();
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

    ret = uv_listen((uv_stream_t *) pipe, 64, new_unix_listener);
    if (ret != 0) {
        std::free(pipe);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_listen failed: {}", uv_strerror(ret));
    }

    m_handle = m_manager->allocateAcceptHandle((uv_stream_t *) pipe, m_options.allowInsecure, std::move(ctx));

    return {};
}

void
chord_mesh::new_tcp4_listener(uv_stream_t *server, int err)
{
    auto *handle = (AcceptHandle *) server->data;

    if (err < 0) {
        handle->error(
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
        handle->error(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_tcp_init error: {}", uv_strerror(err)));
        return;
    }

    auto *client = (uv_stream_t *) tcp;

    ret = uv_accept(server, client);
    if (ret != 0) {
        std::free(tcp);
        handle->error(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_accept error: {}", uv_strerror(err)));
        return;
    }

    auto *manager = handle->manager;
    auto stream = std::make_shared<Stream>(manager->allocateStreamHandle(client, /* initiator= */ false, handle->insecure));
    handle->accept(stream);
}

tempo_utils::Status
chord_mesh::StreamAcceptor::listenTcp4(
     std::string_view ipAddress,
     tu_uint16 tcpPort,
     std::unique_ptr<AbstractAcceptContext> &&ctx)
{
    if (m_handle != nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "acceptor is already listening");

    auto *loop = m_manager->getLoop();
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

    ret = uv_listen((uv_stream_t *) tcp, 64, new_tcp4_listener);
    if (ret != 0) {
        std::free(tcp);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_listen failed: {}", uv_strerror(ret));
    }

    m_handle = m_manager->allocateAcceptHandle((uv_stream_t *) tcp, m_options.allowInsecure, std::move(ctx));

    return {};
}

tempo_utils::Status
chord_mesh::StreamAcceptor::listenLocation(
    const chord_common::TransportLocation &endpoint,
    std::unique_ptr<AbstractAcceptContext> &&ctx)
{
    switch (endpoint.getType()) {
        case chord_common::TransportType::Unix:
            return listenUnix(endpoint.getUnixPath().c_str(), 0, std::move(ctx));
        case chord_common::TransportType::Tcp4:
            return listenTcp4(endpoint.getTcp4Address(), endpoint.getTcp4Port(), std::move(ctx));
        default:
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "invalid acceptor endpoint");
    }
}

void
chord_mesh::StreamAcceptor::shutdown()
{
    if (m_handle == nullptr)
        return;
    m_handle->shutdown();
}