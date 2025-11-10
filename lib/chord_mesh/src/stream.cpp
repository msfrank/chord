
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream.h>
#include <tempo_utils/big_endian.h>

chord_mesh::Stream::Stream(StreamHandle *handle, bool initiator)
    : m_handle(handle),
      m_initiator(initiator),
      m_id(tempo_utils::UUID::randomUUID()),
      m_state(StreamState::Initial),
      m_data(nullptr)
{
    TU_ASSERT (m_handle != nullptr);
    m_handle->data = this;
    m_io = std::make_unique<StreamIO>(this);
}

chord_mesh::Stream::~Stream()
{
    shutdown();
}

tempo_utils::UUID
chord_mesh::Stream::getId() const
{
    return m_id;
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
    auto &io = stream->m_io;
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
    Message message;
    bool hasMore = true;

    while (hasMore) {
        auto status = io->read((const tu_uint8 *) buf->base, nread, message, hasMore);
        std::free(buf->base);

        // invoke the receive callback for each ready message
        if (message.isValid()) {
            ops.receive(message, data);
        }

        // if read returned error then propagate it
        if (status.notOk()) {
            ops.error(status, data);
        }
    }
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

    TU_RETURN_IF_NOT_OK (m_io->start());

    return {};
}

tempo_utils::Status
chord_mesh::Stream::handshake(
    const NoiseProtocolId *protocolId,
    std::span<const tu_uint8> prologue,
    std::shared_ptr<tempo_security::X509Certificate> remoteCertificate,
    std::shared_ptr<tempo_security::PrivateKey> localPrivateKey)
{
    return {};
}

void
chord_mesh::write_completed(uv_write_t *req, int err)
{
    auto *streamBuf = (StreamBuf *) req->data;
    auto *handle = (StreamHandle *) req->handle->data;
    auto *stream = (Stream *) handle->data;

    // we are done with req and streamBuf
    std::free(req);
    free_stream_buf(streamBuf);

    if (err < 0) {
        stream->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "perform_write error: {}", uv_strerror(err)));
        return;
    }

    // TODO: signal write completion to StreamIO
}

tempo_utils::Status
chord_mesh::Stream::write(StreamBuf *streamBuf)
{
    // construct write req which will be freed in write_completed callback
    auto *req = (uv_write_t *) std::malloc(sizeof(uv_write_t));
    memset(req, 0, sizeof(uv_write_t));
    req->data = streamBuf;

    auto ret = uv_write(req, m_handle->stream, &streamBuf->buf, 1, write_completed);
    if (ret != 0) {
        std::free(req); // we free req but leave streamBuf untouched
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "write error: {}", uv_strerror(ret));
    }
    return {};
}

tempo_utils::Status
chord_mesh::Stream::send(std::shared_ptr<const tempo_utils::ImmutableBytes> message)
{
    TU_ASSERT (message != nullptr);
    if (m_state == StreamState::Closed)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "stream is closed");
    TU_RETURN_IF_NOT_OK (m_io->write(message));
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