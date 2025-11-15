#ifndef CHORD_MESH_STREAM_H
#define CHORD_MESH_STREAM_H

#include <queue>

#include <noise/protocol.h>
#include <uv.h>

#include <tempo_utils/bytes_appender.h>
#include <tempo_utils/result.h>
#include <tempo_utils/uuid.h>

#include "message.h"
#include "stream_buf.h"
#include "stream_io.h"
#include "stream_manager.h"

namespace chord_mesh {

    enum class StreamState {
        Initial,
        Active,
        Closed,
    };

    struct StreamOps {
        void (*receive)(const Message &, void *) = nullptr;
        bool (*negotiate)(std::string_view, std::string_view, void *) = nullptr;
        void (*error)(const tempo_utils::Status &, void *) = nullptr;
        void (*cleanup)(void *) = nullptr;
    };

    class Stream : public AbstractStreamBufWriter {
    public:
        Stream(StreamHandle *handle, bool initiator, bool secure);
        virtual ~Stream();

        tempo_utils::UUID getId() const;
        StreamState getStreamState() const;

        tempo_utils::Status start(const StreamOps &ops, void *data = nullptr);
        tempo_utils::Status negotiate(std::string_view protocolName);
        tempo_utils::Status send(
            MessageVersion version,
            std::shared_ptr<const tempo_utils::ImmutableBytes> payload,
            absl::Time timestamp = {});
        void shutdown();

        tempo_utils::Status write(StreamBuf *buf) override;

    private:
        StreamHandle *m_handle;
        bool m_initiator;
        bool m_secure;

        tempo_utils::UUID m_id;
        StreamState m_state;
        StreamOps m_ops;
        void *m_data;
        std::unique_ptr<StreamIO> m_io;

        friend class StreamAcceptor;
        friend class StreamConnector;

        void processReadyMessages();
        void processStreamMessage(const Message &message);
        void emitError(const tempo_utils::Status &status);

        friend void allocate_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
        friend void perform_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
        friend void write_completed(uv_write_t *req, int err);
    };
}

#endif // CHORD_MESH_STREAM_H