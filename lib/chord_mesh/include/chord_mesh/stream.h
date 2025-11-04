#ifndef CHORD_MESH_STREAM_H
#define CHORD_MESH_STREAM_H

#include <queue>

#include <nng/nng.h>
#include <uv.h>

#include <tempo_utils/bytes_appender.h>
#include <tempo_utils/result.h>

namespace chord_mesh {

    struct StreamOps {
        void (*receive)(std::shared_ptr<const tempo_utils::ImmutableBytes>, void *) = nullptr;
        void (*cleanup)(void *) = nullptr;
    };

    class Stream {
    public:
        Stream(uv_loop_t *loop, uv_stream_t *stream);
        virtual ~Stream();

        bool isOk() const;
        tempo_utils::Status getStatus() const;

        tempo_utils::Status start(const StreamOps &ops, void *data = nullptr);
        tempo_utils::Status send(std::shared_ptr<const tempo_utils::ImmutableBytes> message);
        void shutdown();

    private:
        uv_loop_t *m_loop;
        uv_stream_t *m_stream;

        bool m_configured;
        StreamOps m_ops;
        void *m_data;
        tempo_utils::BytesAppender m_incoming;
        tu_uint32 m_msgsize;
        uv_write_t m_req;
        uv_buf_t m_buf;
        std::queue<std::shared_ptr<const tempo_utils::ImmutableBytes>> m_outgoing;
        bool m_writable;
        tempo_utils::Status m_status;

        friend class StreamAcceptor;
        friend class StreamConnector;

        tempo_utils::Status performWrite();

        friend void allocate_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
        friend void perform_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
        friend void perform_write(uv_write_t *req, int status);
    };
}

#endif // CHORD_MESH_STREAM_H