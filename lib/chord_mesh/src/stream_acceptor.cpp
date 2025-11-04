
#include <chord_mesh/stream_acceptor.h>
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream.h>

chord_mesh::StreamAcceptor::StreamAcceptor(uv_loop_t *loop, uv_stream_t *stream)
    : m_loop(loop),
      m_stream(stream),
      m_configured(false),
      m_data(nullptr)
{
    TU_ASSERT (m_loop != nullptr);
    TU_ASSERT (m_stream != nullptr);
}

chord_mesh::StreamAcceptor::~StreamAcceptor()
{
    shutdown();
}

tempo_utils::Result<std::shared_ptr<chord_mesh::StreamAcceptor>>
chord_mesh::StreamAcceptor::forUnix(
     std::string_view pipePath,
     int pipeFlags,
     uv_loop_t *loop)
{
    auto *pipe = (uv_pipe_t *) std::malloc(sizeof(uv_pipe_t));
    memset(pipe, 0, sizeof(uv_pipe_t));

    auto ret = uv_pipe_init(loop, pipe, false);
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
    auto listener = std::shared_ptr<StreamAcceptor>(new StreamAcceptor(loop, stream));
    stream->data = listener.get();

    return listener;
}

bool
chord_mesh::StreamAcceptor::isOk() const
{
    return m_status.isOk();
}

tempo_utils::Status
chord_mesh::StreamAcceptor::getStatus() const
{
    return m_status;
}

void
chord_mesh::new_listener_connection(uv_stream_t *server, int status)
{
    if (status < 0) {
        TU_LOG_WARN_IF (status < 0) << "new_connection error: " << uv_strerror(status);
        return;
    }

    int ret;

    auto *pipe = (uv_pipe_t *) std::malloc(sizeof(uv_pipe_t));
    memset(pipe, 0, sizeof(uv_pipe_t));

    ret = uv_pipe_init(server->loop, pipe, false);
    if (ret != 0) {
        std::free(pipe);
        TU_LOG_WARN << "uv_pipe_init error: " << uv_strerror(ret);
        return;
    }

    auto *client = (uv_stream_t *) pipe;

    ret = uv_accept(server, client);
    if (status != 0) {
        std::free(pipe);
        TU_LOG_WARN << "uv_accept error: " << uv_strerror(ret);
        return;
    }

    auto *acceptor = (StreamAcceptor *) server->data;
    auto &ops = acceptor->m_ops;
    auto stream = std::make_shared<Stream>(server->loop, client);
    ops.accept(stream, acceptor->m_data);
}

tempo_utils::Status
chord_mesh::StreamAcceptor::listen(const StreamAcceptorOps &ops, void *data)
{
    if (m_configured)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "listener poller is already configured");
    if (m_loop == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "listener poller has been shut down");

    auto ret = uv_listen(m_stream, 64, new_listener_connection);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_listen error: {}", uv_strerror(ret));

    m_configured = true;
    m_ops = ops;
    m_data = data;

    return {};
}

static void
free_stream(uv_handle_t *stream)
{
    std::free(stream);
}

void
chord_mesh::StreamAcceptor::shutdown()
{
    if (m_loop == nullptr)
        return;
    m_loop = nullptr;

    uv_close((uv_handle_t *) m_stream, free_stream);

    if (m_ops.cleanup != nullptr) {
        m_ops.cleanup(m_data);
    }
}
