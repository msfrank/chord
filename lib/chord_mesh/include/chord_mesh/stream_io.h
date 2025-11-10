#ifndef CHORD_MESH_STREAM_IO_H
#define CHORD_MESH_STREAM_IO_H

#include <uv.h>
#include <tempo_utils/immutable_bytes.h>
#include <tempo_utils/status.h>

#include "message.h"

namespace chord_mesh {

    enum class IOState {
        Initial,
        Insecure,
        HandshakePending,
        Handshake,
        Secure,
    };

    struct StreamBuf {
        uv_buf_t buf;
        void (*free)(StreamBuf *);

        std::span<const tu_uint8> getSpan() const;
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
        explicit ArrayBuf(std::vector<tu_uint8> &&bytes);
        std::vector<tu_uint8> m_bytes;

        static ArrayBuf *allocate(std::span<const tu_uint8> bytes);
        static ArrayBuf *allocate(std::string_view str);
        static ArrayBuf *allocate(const tu_uint8 *bytes, size_t size);
    };

    class AbstractStreamBehavior {
    public:
        virtual ~AbstractStreamBehavior() = default;
        virtual tempo_utils::Status read(const tu_uint8 *data, ssize_t size, Message &message, bool &hasMore) = 0;
        virtual tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) = 0;
    };

    class InitialStreamBehavior : public AbstractStreamBehavior {
    public:
        InitialStreamBehavior() = default;
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size, Message &message, bool &hasMore) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;

        bool hasOutgoing() const;
        StreamBuf *takeOutgoing();

    private:
        std::queue<StreamBuf *> m_outgoing;
    };

    class InsecureStreamBehavior : public AbstractStreamBehavior {
    public:
        InsecureStreamBehavior() = default;
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size, Message &message, bool &hasMore) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;

    private:
        MessageParser m_parser;
        std::queue<StreamBuf *> m_outgoing;
    };


    class StreamIO {
    public:
        explicit StreamIO(AbstractStreamBufWriter *writer);

        IOState getIOState() const;

        tempo_utils::Status start();

        tempo_utils::Status read(const tu_uint8 *data, ssize_t size, Message &message, bool &hasMore);
        tempo_utils::Status write(StreamBuf *streamBuf);
        tempo_utils::Status write(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes);

    private:
        AbstractStreamBufWriter *m_writer;
        IOState m_state;
        std::unique_ptr<AbstractStreamBehavior> m_behavior;
    };
}

#endif // CHORD_MESH_STREAM_IO_H