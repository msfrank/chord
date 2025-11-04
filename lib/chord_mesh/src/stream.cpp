
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream.h>
#include <tempo_utils/big_endian.h>

chord_mesh::Stream::Stream(uv_loop_t *loop, uv_stream_t *stream)
    : m_loop(loop),
      m_stream(stream),
      m_configured(false),
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
    auto &incoming = stream->m_incoming;
    auto &msgsize = stream->m_msgsize;

    incoming.appendBytes(std::span((const tu_uint8 *) buf->base, buf->len));
    std::free(buf->base);

    if (msgsize == 0) {
        if (incoming.getSize() >= 4) {
            msgsize = tempo_utils::read_u32(incoming.getData());
        }
    }

    std::shared_ptr<const tempo_utils::ImmutableBytes> message;
    if (incoming.getSize() >= msgsize) {
        auto bytes = incoming.finish();
        auto slice = bytes->toSlice();
        message = slice.slice(0, msgsize).toImmutableBytes();
        auto remainder = slice.slice(msgsize, bytes->getSize() - msgsize);
        incoming.appendBytes(remainder.sliceView());
        msgsize = 0;
    }

    if (message != nullptr) {
        auto &ops = stream->m_ops;
        ops.receive(message, stream->m_data);
    }
}

tempo_utils::Status
chord_mesh::Stream::start(const StreamOps &ops, void *data)
{
    if (m_loop == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "stream has been shut down");
    if (m_configured)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "stream is already started");

    m_stream->data = this;

    auto ret = uv_read_start(m_stream, allocate_buffer, perform_read);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_read_start error: {}", uv_strerror(ret));

    m_configured = true;
    m_ops = ops;
    m_data = data;

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

void
chord_mesh::Stream::shutdown()
{
    if (m_loop == nullptr)
        return;
    m_loop = nullptr;

    uv_close((uv_handle_t *) m_stream, free_stream);

    if (m_ops.cleanup != nullptr) {
        m_ops.cleanup(m_data);
    }
}
