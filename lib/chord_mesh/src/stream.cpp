
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream.h>
#include <tempo_utils/big_endian.h>

chord_mesh::Stream::Stream(uv_loop_t *loop, uv_stream_t *stream)
    : m_loop(loop),
      m_stream(stream),
      m_state(StreamState::Initial),
      m_data(nullptr),
      m_writable(false)
{
    TU_ASSERT (m_loop != nullptr);
    TU_ASSERT (m_stream != nullptr);
}

chord_mesh::Stream::~Stream()
{
    shutdown();
}

chord_mesh::StreamState
chord_mesh::Stream::getStreamState() const
{
    return m_state;
}

void
chord_mesh::allocate_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    auto size = suggested_size < 4096? suggested_size : 4096;
    buf->base = (char *) std::malloc(size);
    buf->len = size;
}

void
chord_mesh::perform_read(uv_stream_t *s, ssize_t nread, const uv_buf_t *buf)
{
    auto *stream = (Stream *) s->data;

    if (nread == 0 || buf == nullptr || buf->len < nread)
        return;
    auto &parser = stream->m_parser;

    auto messageReady = parser.appendBytes(std::span((const tu_uint8 *) buf->base, buf->len));
    std::free(buf->base);

    if (!messageReady)
        return;

    auto &ops = stream->m_ops;
    auto data = stream->m_data;
    do {
        ops.receive(parser.takeMessage(), data);
    }
    while (parser.hasMessage());
}

tempo_utils::Status
chord_mesh::Stream::start(const StreamOps &ops, void *data)
{
    if (m_state != StreamState::Initial)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream state");

    m_stream->data = this;

    auto ret = uv_read_start(m_stream, allocate_buffer, perform_read);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_read_start error: {}", uv_strerror(ret));

    m_state = StreamState::Active;
    m_ops = ops;
    m_data = data;

    if (!m_outgoing.empty()) {
        performWrite();
    }

    return {};
}

void
chord_mesh::perform_write(uv_write_t *req, int status)
{
    auto *stream = (Stream *) req->data;
    stream->m_outgoing.pop();
    if (!stream->m_outgoing.empty()) {
        stream->performWrite();
    }
}

tempo_utils::Status
chord_mesh::Stream::performWrite()
{
    auto message = m_outgoing.front();

    m_buf = uv_buf_init((char *) message->getData(), message->getSize());
    m_req.data = this;

    auto ret = uv_write(&m_req, m_stream, &m_buf, 1, perform_write);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "write error: {}", uv_strerror(ret));
    return {};
}

tempo_utils::Status
chord_mesh::Stream::send(std::shared_ptr<const tempo_utils::ImmutableBytes> message)
{
    TU_ASSERT (message != nullptr);
    if (m_state == StreamState::Closed)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "stream is closed");
    bool ready = m_outgoing.empty();
    m_outgoing.push(message);
    if (ready)
        return performWrite();
    return {};
}

static void
free_stream(uv_handle_t *stream)
{
    std::free(stream);
}

static void
shutdown_stream(uv_shutdown_t *req, int status)
{
    uv_close((uv_handle_t *) req->handle, free_stream);
    std::free(req);
}

void
chord_mesh::Stream::shutdown()
{
    switch (m_state) {
        case StreamState::Initial: {
            uv_close((uv_handle_t *) m_stream, free_stream);
            break;
        }
        case StreamState::Active: {
            auto *req = (uv_shutdown_t *) std::malloc(sizeof(uv_shutdown_t));
            memset(req, 0, sizeof(uv_shutdown_t));
            uv_shutdown(req, m_stream, shutdown_stream);
            break;
        }

        //
        default:
            return;
    }

    m_state = StreamState::Closed;

    if (m_ops.cleanup != nullptr) {
        m_ops.cleanup(m_data);
    }
}

bool
chord_mesh::Stream::isOk() const
{
    return m_status.isOk();
}

tempo_utils::Status
chord_mesh::Stream::getStatus() const
{
    return m_status;
}
