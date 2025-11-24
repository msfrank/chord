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

    class StreamConnector : public std::enable_shared_from_this<StreamConnector> {
    public:
        virtual ~StreamConnector();

        static tempo_utils::Result<std::shared_ptr<StreamConnector>> create(
            StreamManager *manager,
            const StreamConnectorOps &ops,
            const StreamConnectorOptions &options = {});

        tempo_utils::Result<tempo_utils::UUID> connectUnix(
            std::string_view pipePath,
            int pipeFlags,
            void *data = nullptr);
        tempo_utils::Result<tempo_utils::UUID> connectTcp4(
            std::string_view ipAddress,
            tu_uint16 tcpPort,
            void *data = nullptr);
        tempo_utils::Result<tempo_utils::UUID> connectLocation(
            const chord_common::TransportLocation &endpoint,
            void *data = nullptr);

        tempo_utils::Status abort(const tempo_utils::UUID &connectId);

        void shutdown();

    private:
        StreamManager *m_manager;
        StreamConnectorOps m_ops;
        StreamConnectorOptions m_options;

        struct ConnectHandle {
            tempo_utils::UUID id;
            uv_connect_t req;
            bool aborted;
            std::shared_ptr<StreamConnector> connector;
            void *data;
        };
        absl::flat_hash_map<tempo_utils::UUID,std::unique_ptr<ConnectHandle>> m_connecting;

        StreamConnector(
            StreamManager *manager,
            const StreamConnectorOps &ops,
            const StreamConnectorOptions &options);
        void emitError(const tempo_utils::Status &status, void *data);

        friend void new_unix_connection(uv_connect_t *req, int err);
        friend void new_tcp4_connection(uv_connect_t *req, int err);
    };
}

#endif // CHORD_MESH_STREAM_CONNECTOR_H