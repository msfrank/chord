#ifndef CHORD_MESH_STREAM_SESSION_H
#define CHORD_MESH_STREAM_SESSION_H

#include "stream_buf.h"
#include "stream_io.h"

namespace chord_mesh {

    struct StreamHandle;

    class StreamSession : public AbstractStreamBufWriter {
    public:
        StreamSession(StreamHandle *handle, bool initiator, bool insecure);

        tempo_utils::Status start();
        tempo_utils::Status negotiate(std::string_view protocolName);
        tempo_utils::Status read(const tu_uint8 *data, ssize_t len);
        tempo_utils::Status write(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes);
        tempo_utils::Status process();

        tempo_utils::Status write(StreamBuf *buf) override;

    private:
        StreamHandle *m_handle;
        bool m_insecure;
        std::unique_ptr<StreamIO> m_io;

        tempo_utils::Status processStreamMessage(const Envelope &envelope);

        friend void allocate_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
        friend void perform_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
        friend void write_completed(uv_write_t *req, int err);
    };
}

#endif // CHORD_MESH_STREAM_SESSION_H