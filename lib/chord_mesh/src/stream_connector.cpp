
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream_connector.h>

chord_mesh::StreamConnector::StreamConnector(
    StreamManager *manager,
    const StreamConnectorOptions &options)
    : m_manager(manager),
      m_options(options)
{
    TU_ASSERT (m_manager != nullptr);
}

chord_mesh::StreamConnector::~StreamConnector()
{
    shutdown();
}

tempo_utils::Result<std::shared_ptr<chord_mesh::StreamConnector>>
chord_mesh::StreamConnector::create(
    StreamManager *manager,
    const StreamConnectorOptions &options)
{
    TU_ASSERT (manager != nullptr);
    return std::shared_ptr<StreamConnector>(new StreamConnector(manager, options));
}

void
chord_mesh::new_unix_connection(uv_connect_t *req, int err)
{
    auto *connect = (ConnectHandle *) req->data;
    auto *manager = connect->manager;

    // take ownership of pipe
    auto *pipe = req->handle;
    req->handle = nullptr;

    // if connect failed then return without allocating a stream
    if (err < 0) {
        switch (err) {
            case UV_ECANCELED:
                // ECANCELED indicates the connect request was aborted, so we don't invoke the error callback
                break;
            default:
                connect->error(
                    MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                        "failed to connect: {}", uv_strerror(err)));
                break;
        }
        std::free(pipe);
        return;
    }

    // otherwise allocate a stream handle (transferring ownership of pipe) and wrap it in a stream
    auto *handle = manager->allocateStreamHandle(pipe);
    auto stream = std::make_shared<Stream>(handle, /* initiator= */ true, !connect->insecure);

    // invoke the connect callback
    connect->connect(stream);
}

tempo_utils::Result<std::shared_ptr<chord_mesh::Connect>>
chord_mesh::StreamConnector::connectUnix(
    std::string_view pipePath,
    int pipeFlags,
    std::unique_ptr<AbstractConnectContext> &&ctx)
{
    auto *loop = m_manager->getLoop();
    int ret;

    auto id = tempo_utils::UUID::randomUUID();

    auto *pipe = (uv_pipe_t *) std::malloc(sizeof(uv_pipe_t));
    memset(pipe, 0, sizeof(uv_pipe_t));
    ret = uv_pipe_init(loop, pipe, false);
    if (ret != 0) {
        std::free(pipe);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_pipe_init failed: {}", uv_strerror(ret));
    }

    auto *req = (uv_connect_t *) std::malloc(sizeof(uv_connect_t));
    memset(req, 0, sizeof(uv_connect_t));
    ret = uv_pipe_connect2(req, pipe, pipePath.data(), pipePath.size(),
        pipeFlags, new_unix_connection);
    if (ret != 0) {
        std::free(req);
        std::free(pipe);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_pipe_connect2 failed: {}", uv_strerror(ret));
    }

    //auto remote = chord_common::TransportLocation::forUnix({}, pipePath);
    auto *handle = m_manager->allocateConnectHandle(req, m_options.startInsecure, std::move(ctx));
    auto connect = std::make_shared<Connect>(handle);

    return connect;
}

void
chord_mesh::new_tcp4_connection(uv_connect_t *req, int err)
{
    auto *connect = (ConnectHandle *) req->data;
    auto *manager = connect->manager;

    // take ownership of tcp
    auto *tcp = req->handle;
    req->handle = nullptr;

    // if connect failed then return without allocating a stream
    if (err < 0) {
        switch (err) {
            case UV_ECANCELED:
                // ECANCELED indicates the connect request was aborted, so we don't invoke the error callback
                break;
            default:
                connect->error(
                    MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                        "failed to connect: {}", uv_strerror(err)));
                break;
        }
        std::free(tcp);
        return;
    }

    // otherwise allocate a stream handle (transferring ownership of tcp) and wrap it in a stream
    auto *handle = manager->allocateStreamHandle(tcp);
    auto stream = std::make_shared<Stream>(handle, /* initiator= */ true, !connect->insecure);

    // invoke the connect callback
    connect->connect(stream);
}

tempo_utils::Result<std::shared_ptr<chord_mesh::Connect>>
chord_mesh::StreamConnector::connectTcp4(
    std::string_view ipAddress,
    tu_uint16 tcpPort,
    std::unique_ptr<AbstractConnectContext> &&ctx)
{
    auto *loop = m_manager->getLoop();
    int ret;

    auto id = tempo_utils::UUID::randomUUID();

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

    auto *req = (uv_connect_t *) std::malloc(sizeof(uv_connect_t));
    memset(req, 0, sizeof(uv_connect_t));
    ret = uv_tcp_connect(req, tcp, (const sockaddr *) &addr, new_tcp4_connection);
    if (ret != 0) {
        std::free(req);
        std::free(tcp);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_tcp_connect failed: {}", uv_strerror(ret));
    }

    auto *handle = m_manager->allocateConnectHandle(req, m_options.startInsecure, std::move(ctx));
    auto connect = std::make_shared<Connect>(handle);

    return connect;
}

tempo_utils::Result<std::shared_ptr<chord_mesh::Connect>>
chord_mesh::StreamConnector::connectLocation(
    const chord_common::TransportLocation &endpoint,
    std::unique_ptr<AbstractConnectContext> &&ctx)
{
    switch (endpoint.getType()) {
        case chord_common::TransportType::Unix:
            return connectUnix(endpoint.getUnixPath().c_str(), 0, std::move(ctx));
        case chord_common::TransportType::Tcp4:
            return connectTcp4(endpoint.getTcp4Address(), endpoint.getTcp4Port(), std::move(ctx));
        default:
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "invalid connect endpoint");
    }
}

void
chord_mesh::StreamConnector::shutdown()
{
}