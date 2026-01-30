
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream_connector.h>

chord_mesh::StreamConnector::StreamConnector(
    StreamManager *manager,
    const StreamConnectorOps &ops,
    const StreamConnectorOptions &options)
    : m_manager(manager),
      m_ops(ops),
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
    const StreamConnectorOps &ops,
    const StreamConnectorOptions &options)
{
    TU_ASSERT (manager != nullptr);
    return std::shared_ptr<StreamConnector>(new StreamConnector(manager, ops, options));
}

void
chord_mesh::new_unix_connection(uv_connect_t *req, int err)
{
    auto *connectingPtr = (StreamConnector::ConnectHandle *) req->data;
    auto *pipe = req->handle;
    auto connector = connectingPtr->connector;
    auto &ops = connector->m_ops;
    auto &options = connector->m_options;

    // remove connect handle from map
    auto connectingEntry = connector->m_connecting.extract(connectingPtr->id);
    TU_ASSERT (!connectingEntry.empty());
    auto connecting = std::move(connectingEntry.mapped());
    auto *data = connecting->data? connecting->data : options.data;

    // if connect failed then return without allocating a stream
    if (err < 0) {
        switch (err) {
            case UV_ECANCELED:
                // ECANCELED indicates the connect request was aborted, so we don't invoke the error callback
                break;
            default:
                connector->emitError(
                    MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                        "failed to connect: {}", uv_strerror(err)), data);
                break;
        }
        std::free(pipe);
        return;
    }

    // otherwise allocate a stream handle and wrap it in a stram
    auto *manager = connector->m_manager;
    auto *handle = manager->allocateHandle(pipe);
    auto stream = std::make_shared<Stream>(handle, /* initiator= */ true, !options.startInsecure);

    // invoke the connect callback
    ops.connect(stream, data);
}

tempo_utils::Result<tempo_utils::UUID>
chord_mesh::StreamConnector::connectUnix(std::string_view pipePath, int pipeFlags, void *data)
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

    auto connecting = std::make_unique<ConnectHandle>();
    connecting->id = id;
    connecting->req.data = connecting.get();
    connecting->aborted = false;
    connecting->connector = shared_from_this();
    connecting->data = data;
    auto *req = &connecting->req;

    ret = uv_pipe_connect2(req, pipe, pipePath.data(), pipePath.size(),
        pipeFlags, new_unix_connection);
    if (ret != 0) {
        std::free(pipe);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_pipe_connect2 failed: {}", uv_strerror(ret));
    }

    m_connecting[id] = std::move(connecting);

    return id;
}

void
chord_mesh::new_tcp4_connection(uv_connect_t *req, int err)
{
    auto connectingPtr = (StreamConnector::ConnectHandle *) req->data;
    auto *tcp = req->handle;
    auto connector = connectingPtr->connector;
    auto &ops = connector->m_ops;
    auto &options = connector->m_options;

    // remove connect handle from map
    auto connectingEntry = connector->m_connecting.extract(connectingPtr->id);
    TU_ASSERT (!connectingEntry.empty());
    auto connecting = std::move(connectingEntry.mapped());
    auto *data = connecting->data? connecting->data : options.data;

    // if connect failed then return without allocating a stream
    if (err < 0) {
        switch (err) {
            case UV_ECANCELED:
                // ECANCELED indicates the connect request was aborted, so we don't invoke the error callback
                break;
            default:
                connector->emitError(
                    MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                        "failed to connect: {}", uv_strerror(err)), data);
                break;
        }
        std::free(tcp);
        return;
    }

    // otherwise allocate a stream handle and wrap it in a stram
    auto *manager = connecting->connector->m_manager;
    auto *handle = manager->allocateHandle(tcp);
    auto stream = std::make_shared<Stream>(handle, /* initiator= */ true, !options.startInsecure);

    // invoke the connect callback
    ops.connect(stream, data);
}

tempo_utils::Result<tempo_utils::UUID>
chord_mesh::StreamConnector::connectTcp4(std::string_view ipAddress, tu_uint16 tcpPort, void *data)
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

    auto connecting = std::make_unique<ConnectHandle>();
    connecting->id = id;
    connecting->req.data = connecting.get();
    connecting->aborted = false;
    connecting->connector = shared_from_this();
    connecting->data = data;
    auto *req = &connecting->req;

    ret = uv_tcp_connect(req, tcp, (const sockaddr *) &addr, new_unix_connection);
    if (ret != 0) {
        std::free(tcp);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_tcp_connect failed: {}", uv_strerror(ret));
    }

    m_connecting[id] = std::move(connecting);

    return id;
}

tempo_utils::Result<tempo_utils::UUID>
chord_mesh::StreamConnector::connectLocation(const chord_common::TransportLocation &endpoint, void *data)
{
    switch (endpoint.getType()) {
        case chord_common::TransportType::Unix:
            return connectUnix(endpoint.getUnixPath().c_str(), 0, data);
        case chord_common::TransportType::Tcp4:
            return connectTcp4(endpoint.getTcp4Address(), endpoint.getTcp4Port(), data);
        default:
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "invalid connect endpoint");
    }
}

tempo_utils::Status
chord_mesh::StreamConnector::abort(const tempo_utils::UUID &connectId)
{
    auto connectingEntry = m_connecting.find(connectId);
    if (connectingEntry == m_connecting.cend())
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "pending connection {} does not exist", connectId.toString());
    auto &connecting = connectingEntry->second;

    if (!connecting->aborted) {
        connecting->aborted = true;
        uv_close((uv_handle_t *) connecting->req.handle, nullptr);
    }

    return {};
}

void
chord_mesh::StreamConnector::shutdown()
{
    for (const auto &connectingEntry : m_connecting) {
        abort(connectingEntry.first);
    }
}

void
chord_mesh::StreamConnector::emitError(const tempo_utils::Status &status, void *data)
{
    if (m_ops.error != nullptr) {
        m_ops.error(status, data);
    }
}