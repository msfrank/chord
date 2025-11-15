#ifndef CHORD_MESH_STREAM_CONNECTOR_H
#define CHORD_MESH_STREAM_CONNECTOR_H

#include <queue>

#include <uv.h>

#include <tempo_utils/result.h>

#include "stream.h"
#include "stream_manager.h"
#include "chord_common/transport_location.h"

namespace chord_mesh {

    struct StreamConnectorOps {
        void (*connect)(std::shared_ptr<Stream>, void *) = nullptr;
        void (*error)(const tempo_utils::Status &, void *) = nullptr;
        void (*cleanup)(void *) = nullptr;
    };

    struct StreamConnectorOptions {
        bool startInsecure = false;
        void *data = nullptr;
    };

    class StreamConnector {
    public:
        StreamConnector(
            StreamManager *manager,
            const StreamConnectorOps &ops,
            const StreamConnectorOptions &options = {});
        virtual ~StreamConnector();

        tempo_utils::Status connectUnix(std::string_view pipePath, int pipeFlags, void *data = nullptr);
        tempo_utils::Status connectTcp4(std::string_view ipAddress, tu_uint16 tcpPort, void *data = nullptr);
        tempo_utils::Status connectLocation(const chord_common::TransportLocation &endpoint, void *data = nullptr);

    private:
        StreamManager *m_manager;
        StreamConnectorOps m_ops;
        StreamConnectorOptions m_options;

        void emitError(const tempo_utils::Status &status);

        friend void new_unix_connection(uv_connect_t *req, int err);
        friend void new_tcp4_connection(uv_connect_t *req, int err);
    };
}

#endif // CHORD_MESH_STREAM_CONNECTOR_H