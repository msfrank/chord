
#include <chord_mesh/stream_connector.h>

#include "chord_mesh/mesh_result.h"

chord_mesh::StreamConnector::StreamConnector(StreamManager *manager, const StreamConnectorOps &ops, void *data)
    : m_manager(manager),
      m_ops(ops),
      m_data(data)
{
    TU_ASSERT (m_manager != nullptr);
}

chord_mesh::StreamConnector::~StreamConnector()
{
}

struct ConnectUnixData {
    uv_connect_t req;
    chord_mesh::StreamConnector *connector;
    void *data;
};

void
chord_mesh::new_unix_connection(uv_connect_t *req, int err)
{
    auto connect = (ConnectUnixData *) req->data;
    auto *pipe = req->handle;
    auto *connector = connect->connector;
    auto &ops = connector->m_ops;

    if (err < 0) {
        connector->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "failed to connect: {}", uv_strerror(err)));
        std::free(pipe);
        std::free(connector);
        return;
    }

    auto *manager = connect->connector->m_manager;
    auto *handle = manager->allocateHandle(pipe);
    auto stream = std::make_shared<Stream>(handle, /* initiator= */ true);
    auto *data = connect->data? connect->data : connect->connector->m_data;

    ops.connect(stream, data);
}

tempo_utils::Status
chord_mesh::StreamConnector::connectUnix(std::string_view pipePath, int pipeFlags, void *data)
{
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

    auto *connect = (ConnectUnixData *) std::malloc(sizeof(ConnectUnixData));
    memset(connect, 0, sizeof(ConnectUnixData));
    connect->req.data = connect;
    connect->connector = this;
    connect->data = data;
    auto *req = &connect->req;

    ret = uv_pipe_connect2(req, pipe, pipePath.data(), pipePath.size(),
        pipeFlags, new_unix_connection);
    if (ret != 0) {
        std::free(connect);
        std::free(pipe);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_pipe_connect2 failed: {}", uv_strerror(ret));
    }

    return {};
}

struct ConnectTcp4Data {
    uv_connect_t req;
    chord_mesh::StreamConnector *connector;
    void *data;
};

void
chord_mesh::new_tcp4_connection(uv_connect_t *req, int err)
{
    auto connect = (ConnectTcp4Data *) req->data;
    auto *tcp = req->handle;
    auto *connector = connect->connector;
    auto &ops = connector->m_ops;

    if (err < 0) {
        connector->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "failed to connect: {}", uv_strerror(err)));
        std::free(tcp);
        std::free(connect);
        return;
    }

    auto *manager = connect->connector->m_manager;
    auto *handle = manager->allocateHandle(tcp);
    auto stream = std::make_shared<Stream>(handle, /* initiator= */ true);
    auto *data = connect->data? connect->data : connect->connector->m_data;

    ops.connect(stream, data);
}

tempo_utils::Status
chord_mesh::StreamConnector::connectTcp4(std::string_view ipAddress, tu_uint16 tcpPort, void *data)
{
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

    auto *connect = (ConnectTcp4Data *) std::malloc(sizeof(ConnectTcp4Data));
    memset(connect, 0, sizeof(ConnectTcp4Data));
    connect->req.data = connect;
    connect->connector = this;
    connect->data = data;
    auto *req = &connect->req;

    ret = uv_tcp_connect(req, tcp, (const sockaddr *) &addr, new_unix_connection);
    if (ret != 0) {
        std::free(connect);
        std::free(tcp);
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_tcp_connect failed: {}", uv_strerror(ret));
    }

    return {};
}

tempo_utils::Status
chord_mesh::StreamConnector::connectLocation(const chord_common::TransportLocation &endpoint, void *data)
{
    switch (endpoint.getType()) {
        case chord_common::TransportType::Unix:
            return connectUnix(endpoint.getUnixPath().c_str(), 0, data);
        default:
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "invalid connect endpoint");
    }
}

void
chord_mesh::StreamConnector::emitError(const tempo_utils::Status &status)
{
    if (m_ops.error != nullptr) {
        m_ops.error(status, m_data);
    }
}