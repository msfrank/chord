#ifndef CHORD_MESH_STREAM_CONNECTOR_H
#define CHORD_MESH_STREAM_CONNECTOR_H

#include <queue>

#include <nng/nng.h>
#include <uv.h>

#include <tempo_utils/result.h>

#include "stream.h"

namespace chord_mesh {

    struct StreamConnectorOps {
        void (*connect)(std::shared_ptr<Stream>, void *) = nullptr;
        void (*cleanup)(void *) = nullptr;
    };

    class StreamConnector {
    public:
        StreamConnector(uv_loop_t *loop, const StreamConnectorOps &ops, void *data = nullptr);
        virtual ~StreamConnector();

        tempo_utils::Status connectUnix(std::string_view pipePath, int pipeFlags);

    private:
        uv_loop_t *m_loop;
        StreamConnectorOps m_ops;
        void *m_data;

        friend void new_unix_connection(uv_connect_t *req, int status);
    };
}

#endif // CHORD_MESH_STREAM_CONNECTOR_H