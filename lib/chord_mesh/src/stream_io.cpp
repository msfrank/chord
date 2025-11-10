
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream_io.h>

std::span<const tu_uint8>
chord_mesh::StreamBuf::getSpan() const
{
    return std::span((const tu_uint8 *) buf.base, buf.len);
}

void
chord_mesh::free_stream_buf(void *ptr)
{
    auto *streamBuf = (StreamBuf *) ptr;
    if (streamBuf->free != nullptr) {
        streamBuf->free(streamBuf);
    }
}

chord_mesh::ImmutableBytesBuf::ImmutableBytesBuf(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes)
    : StreamBuf(),
      m_bytes(std::move(bytes))
{
    buf = uv_buf_init((char *) m_bytes->getData(), m_bytes->getSize());
}

static void
free_immutable_bytes_buf(chord_mesh::StreamBuf *streamBuf)
{
    auto *immutableBytesBuf = static_cast<chord_mesh::ImmutableBytesBuf *>(streamBuf);
    delete immutableBytesBuf;
}

chord_mesh::ImmutableBytesBuf *
chord_mesh::ImmutableBytesBuf::allocate(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes)
{
    auto *buf = new ImmutableBytesBuf(std::move(bytes));
    buf->free = free_immutable_bytes_buf;
    return buf;
}

chord_mesh::ArrayBuf::ArrayBuf(std::vector<tu_uint8> &&bytes)
    : m_bytes(std::move(bytes))
{
}

static void
free_array_buf(chord_mesh::StreamBuf *streamBuf)
{
    auto *arrayBuf = static_cast<chord_mesh::ArrayBuf *>(streamBuf);
    delete arrayBuf;
}

chord_mesh::ArrayBuf *
chord_mesh::ArrayBuf::allocate(std::span<const tu_uint8> bytes)
{
    std::vector copy(bytes.begin(), bytes.end());
    auto *buf = new ArrayBuf(std::move(copy));
    buf->free = free_array_buf;
    return buf;
}

chord_mesh::ArrayBuf *
chord_mesh::ArrayBuf::allocate(const tu_uint8 *bytes, size_t size)
{
    std::vector<tu_uint8> copy(bytes, bytes + size);
    auto *buf = new ArrayBuf(std::move(copy));
    buf->free = free_array_buf;
    return buf;
}

chord_mesh::ArrayBuf *
chord_mesh::ArrayBuf::allocate(std::string_view str)
{
    return allocate((const tu_uint8 *) str.data(), str.size());
}

tempo_utils::Status
chord_mesh::InitialStreamBehavior::read(const tu_uint8 *data, ssize_t size, Message &message, bool &hasMore)
{
    return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
        "stream IO cannot read in Initial state");
}

tempo_utils::Status
chord_mesh::InitialStreamBehavior::write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf)
{
    m_outgoing.push(streamBuf);
    return {};
}

bool
chord_mesh::InitialStreamBehavior::hasOutgoing() const
{
    return !m_outgoing.empty();
}

chord_mesh::StreamBuf *
chord_mesh::InitialStreamBehavior::takeOutgoing()
{
    if (m_outgoing.empty())
        return nullptr;
    auto outgoing = m_outgoing.front();
    m_outgoing.pop();
    return outgoing;
}

tempo_utils::Status
chord_mesh::InsecureStreamBehavior::read(const tu_uint8 *data, ssize_t size, Message &message, bool &hasMore)
{
    std::span bytes(data, size);
    auto pushResult = m_parser.pushBytes(bytes);

    if (pushResult.isStatus()) {
        hasMore = false;
        m_parser.reset();
        return pushResult.getStatus();
    }

    message = pushResult.getResult();
    hasMore = m_parser.hasPending();
    return {};
}

tempo_utils::Status
chord_mesh::InsecureStreamBehavior::write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf)
{
    auto status = writer->write(streamBuf);
    if (status.notOk()) {
        free_stream_buf(streamBuf);
    }
    return status;
}

chord_mesh::StreamIO::StreamIO(AbstractStreamBufWriter *writer)
    : m_writer(writer),
      m_state(IOState::Initial),
      m_behavior(std::make_unique<InitialStreamBehavior>())
{
    TU_ASSERT (m_writer != nullptr);
}

chord_mesh::IOState
chord_mesh::StreamIO::getIOState() const
{
    return m_state;
}

tempo_utils::Status
chord_mesh::StreamIO::start()
{
    if (m_state != IOState::Initial)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream IO state");

    auto prev = std::move(m_behavior);
    auto *initial = (InitialStreamBehavior *) prev.get();

    m_behavior = std::make_unique<InsecureStreamBehavior>();
    auto *insecure = (InsecureStreamBehavior *) m_behavior.get();

    while (initial->hasOutgoing()) {
        auto *streamBuf = initial->takeOutgoing();
        TU_RETURN_IF_NOT_OK (insecure->write(m_writer, streamBuf));
    }
    return {};
}

tempo_utils::Status
chord_mesh::StreamIO::read(const tu_uint8 *data, ssize_t size, Message &message, bool &hasMore)
{
    if (m_behavior == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream IO state");
    return m_behavior->read(data, size, message, hasMore);
}

tempo_utils::Status
chord_mesh::StreamIO::write(StreamBuf *streamBuf)
{
    if (m_behavior == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream IO state");
    return m_behavior->write(m_writer, streamBuf);
}

tempo_utils::Status
chord_mesh::StreamIO::write(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes)
{
    auto *streamBuf = ImmutableBytesBuf::allocate(std::move(bytes));
    auto status = write(streamBuf);
    if (status.notOk()) {
        free_immutable_bytes_buf(streamBuf);
    }
    return status;
}
