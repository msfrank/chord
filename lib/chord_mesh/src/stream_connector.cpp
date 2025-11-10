
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
    auto connect = std::unique_ptr<ConnectUnixData>((ConnectUnixData *) req->data);
    auto *connector = connect->connector;
    auto &ops = connector->m_ops;

    if (err < 0) {
        connector->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "failed to connect: {}", uv_strerror(err)));
        return;
    }

    auto *manager = connect->connector->m_manager;
    auto *handle = manager->allocateHandle(req->handle);
    auto stream = std::make_shared<Stream>(handle, /* initiator= */ true);
    auto *data = connect->data? connect->data : connect->connector->m_data;

    ops.connect(stream, data);
}

tempo_utils::Status
chord_mesh::StreamConnector::connectUnix(std::string_view pipePath, int pipeFlags, void *data)
{
    auto *loop = m_manager->getLoop();

    auto pipe = std::make_unique<uv_pipe_t>();
    auto ret = uv_pipe_init(loop, pipe.get(), false);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_pipe_init failed: {}", uv_strerror(ret));

    auto connect = std::make_unique<ConnectUnixData>();
    connect->req.data = connect.get();
    connect->connector = this;
    connect->data = data;
    auto *req = &connect->req;

    ret = uv_pipe_connect2(req, pipe.get(), pipePath.data(), pipePath.size(),
        pipeFlags, new_unix_connection);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_pipe_connect2 failed: {}", uv_strerror(ret));
    connect.release();

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