#ifndef CHORD_MESH_STREAM_CONNECTOR_H
#define CHORD_MESH_STREAM_CONNECTOR_H

#include <uv.h>

#include <chord_common/transport_location.h>
#include <tempo_utils/result.h>

#include "connect.h"
#include "stream.h"
#include "stream_manager.h"

namespace chord_mesh {

    struct StreamConnectorOptions {
        bool startInsecure = false;
    };

    class StreamConnector : public std::enable_shared_from_this<StreamConnector> {
    public:
        virtual ~StreamConnector();

        static tempo_utils::Result<std::shared_ptr<StreamConnector>> create(
            StreamManager *manager,
            const StreamConnectorOptions &options = {});

        tempo_utils::Result<std::shared_ptr<Connect>> connectUnix(
            std::string_view pipePath,
            int pipeFlags,
            std::unique_ptr<AbstractConnectContext> &&ctx);
        tempo_utils::Result<std::shared_ptr<Connect>> connectTcp4(
            std::string_view ipAddress,
            tu_uint16 tcpPort,
            std::unique_ptr<AbstractConnectContext> &&ctx);
        tempo_utils::Result<std::shared_ptr<Connect>> connectLocation(
            const chord_common::TransportLocation &endpoint,
            std::unique_ptr<AbstractConnectContext> &&ctx);

        void shutdown();

    private:
        StreamManager *m_manager;
        StreamConnectorOptions m_options;

        StreamConnector(StreamManager *manager, const StreamConnectorOptions &options);

        friend void new_unix_connection(uv_connect_t *req, int err);
        friend void new_tcp4_connection(uv_connect_t *req, int err);
    };
}

#endif // CHORD_MESH_STREAM_CONNECTOR_H