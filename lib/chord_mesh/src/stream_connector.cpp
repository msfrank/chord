
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
};

void
chord_mesh::new_unix_connection(uv_connect_t *req, int status)
{
    auto data = std::unique_ptr<ConnectUnixData>((ConnectUnixData *) req->data);
    auto &ops = data->connector->m_ops;
    auto *manager = data->connector->m_manager;
    auto *handle = manager->allocateHandle(req->handle);
    auto stream = std::make_shared<Stream>(handle);
    ops.connect(stream, data->connector->m_data);
}

tempo_utils::Status
chord_mesh::StreamConnector::connectUnix(std::string_view pipePath, int pipeFlags)
{
    auto *loop = m_manager->getLoop();

    auto pipe = std::make_unique<uv_pipe_t>();
    auto ret = uv_pipe_init(loop, pipe.get(), false);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_pipe_init failed: {}", uv_strerror(ret));

    auto data = std::make_unique<ConnectUnixData>();
    data->req.data = data.get();
    data->connector = this;
    auto *req = &data->req;

    ret = uv_pipe_connect2(req, pipe.get(), pipePath.data(), pipePath.size(),
        pipeFlags, new_unix_connection);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_pipe_connect2 failed: {}", uv_strerror(ret));
    data.release();

    return {};
}