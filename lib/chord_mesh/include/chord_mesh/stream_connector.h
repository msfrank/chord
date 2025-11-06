#ifndef CHORD_MESH_STREAM_CONNECTOR_H
#define CHORD_MESH_STREAM_CONNECTOR_H

#include <queue>

#include <uv.h>

#include <tempo_utils/result.h>

#include "stream.h"
#include "stream_manager.h"

namespace chord_mesh {

    struct StreamConnectorOps {
        void (*connect)(std::shared_ptr<Stream>, void *) = nullptr;
        void (*error)(const tempo_utils::Status &, void *) = nullptr;
        void (*cleanup)(void *) = nullptr;
    };

    class StreamConnector {
    public:
        StreamConnector(StreamManager *manager, const StreamConnectorOps &ops, void *data = nullptr);
        virtual ~StreamConnector();

        tempo_utils::Status connectUnix(std::string_view pipePath, int pipeFlags);

    private:
        StreamManager *m_manager;
        StreamConnectorOps m_ops;
        void *m_data;

        void emitError(const tempo_utils::Status &status);

        friend void new_unix_connection(uv_connect_t *req, int err);
    };
}

#endif // CHORD_MESH_STREAM_CONNECTOR_H