
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream.h>
#include <tempo_utils/big_endian.h>

chord_mesh::Stream::Stream(StreamHandle *handle)
    : m_handle(handle),
      m_state(StreamState::Initial),
      m_data(nullptr)
{
    TU_ASSERT (m_handle != nullptr);
    m_handle->data = this;
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
    auto *handle = (StreamHandle *) s->data;
    auto *stream = (Stream *) handle->data;
    auto &ops = stream->m_ops;
    auto data = stream->m_data;

    // if the remote end has closed then shut down the stream
    if (nread == UV_EOF) {
        stream->shutdown();
        return;
    }

    // if nread is 0 then there is no data to read
    if (nread == 0 || buf == nullptr || buf->len < nread)
        return;

    // push data into the message parser
    auto &parser = stream->m_parser;
    std::span bytes((const tu_uint8 *) buf->base, buf->len);
    auto status = parser.pushBytes(bytes);
    std::free(buf->base);

    //
    if (status.notOk()) {
        ops.error(status, data);
    }

    // a complete message has not yet been assembled
    if (!parser.hasMessage())
        return;

    // otherwise there is at least 1 ready message, so loop invoking the
    // receive callback until there are no more ready messages
    do {
        ops.receive(parser.popMessage(), data);
    }
    while (parser.hasMessage());
}

tempo_utils::Status
chord_mesh::Stream::start(const StreamOps &ops, void *data)
{
    if (m_state != StreamState::Initial)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream state");

    auto ret = uv_read_start(m_handle->stream, allocate_buffer, perform_read);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_read_start error: {}", uv_strerror(ret));

    m_state = StreamState::Active;
    m_ops = ops;
    m_data = data;

    if (!m_outgoing.empty())
        return performWrite();

    return {};
}

void
chord_mesh::perform_write(uv_write_t *req, int err)
{
    auto *handle = (StreamHandle *) req->handle->data;
    auto *stream = (Stream *) handle->data;

    if (err < 0) {
        stream->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "perform_write error: {}", uv_strerror(err)));
        return;
    }

    stream->m_outgoing.pop();
    if (!stream->m_outgoing.empty()) {
        auto status = stream->performWrite();
        if (status.notOk()) {
            stream->emitError(status);
        }
    }
}

tempo_utils::Status
chord_mesh::Stream::performWrite()
{
    auto message = m_outgoing.front();

    m_buf = uv_buf_init((char *) message->getData(), message->getSize());

    auto ret = uv_write(&m_req, m_handle->stream, &m_buf, 1, perform_write);
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

void
chord_mesh::Stream::shutdown()
{
    switch (m_state) {
        case StreamState::Initial:
            m_handle->close();
            break;
        case StreamState::Active:
            m_handle->shutdown();
            break;
        default:
            return;
    }

    m_state = StreamState::Closed;

    if (m_ops.cleanup != nullptr) {
        m_ops.cleanup(m_data);
    }
}

void
chord_mesh::Stream::emitError(const tempo_utils::Status &status)
{
    if (m_ops.error != nullptr) {
        m_ops.error(status, m_data);
    }
}