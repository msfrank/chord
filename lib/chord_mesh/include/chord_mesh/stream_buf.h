#ifndef CHORD_MESH_STREAM_BUF_H
#define CHORD_MESH_STREAM_BUF_H

#include <uv.h>

#include <tempo_utils/immutable_bytes.h>
#include <tempo_utils/status.h>

namespace chord_mesh {

    struct StreamBuf {
        uv_buf_t buf;
        void (*free)(StreamBuf *);

        std::span<const tu_uint8> getSpan() const;
        std::string_view getStringView() const;
    };

    class AbstractStreamBufWriter {
    public:
        virtual ~AbstractStreamBufWriter() = default;
        virtual tempo_utils::Status write(StreamBuf *buf) = 0;
    };

    void free_stream_buf(void *streamBuf);

    struct ImmutableBytesBuf : StreamBuf {
        explicit ImmutableBytesBuf(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes);
        std::shared_ptr<const tempo_utils::ImmutableBytes> m_bytes;

        static ImmutableBytesBuf *allocate(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes);
    };

    struct ArrayBuf : StreamBuf {
        explicit ArrayBuf(size_t size);
        explicit ArrayBuf(std::vector<tu_uint8> &&bytes);
        std::vector<tu_uint8> m_bytes;

        static ArrayBuf *allocate(size_t size);
        static ArrayBuf *allocate(std::span<const tu_uint8> bytes);
        static ArrayBuf *allocate(std::string_view str);
        static ArrayBuf *allocate(const tu_uint8 *bytes, size_t size);
    };
}

#endif // CHORD_MESH_STREAM_BUF_H