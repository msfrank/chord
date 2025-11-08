#ifndef CHORD_MESH_STREAM_H
#define CHORD_MESH_STREAM_H

#include <queue>

#include <uv.h>

#include <tempo_utils/bytes_appender.h>
#include <tempo_utils/result.h>
#include <tempo_utils/uuid.h>

#include "message.h"
#include "stream_manager.h"

namespace chord_mesh {

    enum class StreamState {
        Initial,
        Active,
        Closed,
    };

    struct StreamOps {
        void (*receive)(const Message &, void *) = nullptr;
        void (*error)(const tempo_utils::Status &, void *) = nullptr;
        void (*cleanup)(void *) = nullptr;
    };

    class Stream {
    public:
        explicit Stream(StreamHandle *handle);
        virtual ~Stream();

        tempo_utils::UUID getId() const;
        StreamState getStreamState() const;

        tempo_utils::Status start(const StreamOps &ops, void *data = nullptr);
        tempo_utils::Status send(std::shared_ptr<const tempo_utils::ImmutableBytes> message);
        void shutdown();

    private:
        StreamHandle *m_handle;

        tempo_utils::UUID m_id;
        StreamState m_state;
        StreamOps m_ops;
        void *m_data;
        MessageParser m_parser;
        uv_write_t m_req;
        uv_buf_t m_buf;
        std::queue<std::shared_ptr<const tempo_utils::ImmutableBytes>> m_outgoing;

        friend class StreamAcceptor;
        friend class StreamConnector;

        tempo_utils::Status performWrite();
        void emitError(const tempo_utils::Status &status);

        friend void allocate_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
        friend void perform_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
        friend void perform_write(uv_write_t *req, int err);
    };
}

#endif // CHORD_MESH_STREAM_H