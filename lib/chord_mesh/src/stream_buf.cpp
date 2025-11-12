
#include <chord_mesh/stream_buf.h>

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

chord_mesh::ArrayBuf::ArrayBuf(size_t size)
    : m_bytes(size)
{
    buf = uv_buf_init((char *) m_bytes.data(), m_bytes.size());
}

chord_mesh::ArrayBuf::ArrayBuf(std::vector<tu_uint8> &&bytes)
    : m_bytes(std::move(bytes))
{
    buf = uv_buf_init((char *) m_bytes.data(), m_bytes.size());
}

static void
free_array_buf(chord_mesh::StreamBuf *streamBuf)
{
    auto *arrayBuf = static_cast<chord_mesh::ArrayBuf *>(streamBuf);
    delete arrayBuf;
}

chord_mesh::ArrayBuf *
chord_mesh::ArrayBuf::allocate(size_t size)
{
    auto *buf = new ArrayBuf(size);
    buf->free = free_array_buf;
    return buf;
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
