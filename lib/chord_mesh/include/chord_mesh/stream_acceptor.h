#ifndef CHORD_MESH_STREAM_ACCEPTOR_H
#define CHORD_MESH_STREAM_ACCEPTOR_H

#include <queue>

#include <uv.h>

#include <tempo_utils/result.h>

#include "stream.h"

namespace chord_mesh {

    struct StreamAcceptorOps {
        void (*accept)(std::shared_ptr<Stream>, void *) = nullptr;
        void (*cleanup)(void *) = nullptr;
    };

    class StreamAcceptor {
    public:
        virtual ~StreamAcceptor();

        static tempo_utils::Result<std::shared_ptr<StreamAcceptor>> forUnix(
            std::string_view pipePath,
            int pipeFlags,
            uv_loop_t *loop);

        bool isOk() const;
        tempo_utils::Status getStatus() const;

        tempo_utils::Status listen(const StreamAcceptorOps &ops, void *data = nullptr);
        void shutdown();

    private:
        uv_loop_t *m_loop;
        uv_stream_t *m_stream;

        bool m_configured;
        StreamAcceptorOps m_ops;
        void *m_data;
        tempo_utils::Status m_status;

        StreamAcceptor(uv_loop_t *loop, uv_stream_t *stream);

        friend void new_listener_connection(uv_stream_t *server, int status);
    };
}

#endif // CHORD_MESH_STREAM_ACCEPTOR_H