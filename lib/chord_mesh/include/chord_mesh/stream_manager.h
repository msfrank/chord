#ifndef CHORD_MESH_STREAM_MANAGER_H
#define CHORD_MESH_STREAM_MANAGER_H

#include <uv.h>

#include <tempo_utils/result.h>

namespace chord_mesh {

    class StreamManager;

    struct StreamHandle {
        uv_stream_t *stream;
        StreamManager *manager;
        void *data;
        StreamHandle *prev;
        StreamHandle *next;

        StreamHandle(uv_stream_t *stream, StreamManager *manager, void *data);
        void shutdown();
        void close();

    private:
        uv_shutdown_t m_req;
        bool m_closing;
    };

    class StreamManager {
    public:
        explicit StreamManager(uv_loop_t *loop);

        uv_loop_t *getLoop() const;

        StreamHandle *allocateHandle(uv_stream_t *stream, void *data = nullptr);

    private:
        uv_loop_t *m_loop;
        StreamHandle *m_handles;

        void freeHandle(StreamHandle *handle);

        friend struct StreamHandle;
        friend void shutdown_stream(uv_shutdown_t *req, int status);
        friend void close_stream(uv_handle_t *stream);
    };
}

#endif // CHORD_MESH_STREAM_MANAGER_H