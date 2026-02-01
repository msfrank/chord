#ifndef CHORD_MESH_STREAM_ACCEPTOR_H
#define CHORD_MESH_STREAM_ACCEPTOR_H

#include <uv.h>

#include <chord_common/transport_location.h>
#include <tempo_utils/result.h>

#include "stream.h"

namespace chord_mesh {

    struct StreamAcceptorOptions {
        bool allowInsecure = false;
    };

    class StreamAcceptor : public std::enable_shared_from_this<StreamAcceptor> {

        struct Private{ explicit Private() = default; };

    public:
        StreamAcceptor(StreamManager *manager, const StreamAcceptorOptions &options, Private);
        virtual ~StreamAcceptor();

        static tempo_utils::Result<std::shared_ptr<StreamAcceptor>> create(
            StreamManager *manager,
            const StreamAcceptorOptions &options = {});

        AcceptState getAcceptState() const;

        tempo_utils::Status listenUnix(
            std::string_view pipePath,
            int pipeFlags,
            std::unique_ptr<AbstractAcceptContext> &&ctx);
        tempo_utils::Status listenTcp4(
            std::string_view ipAddress,
            tu_uint16 tcpPort,
            std::unique_ptr<AbstractAcceptContext> &&ctx);
        tempo_utils::Status listenLocation(
            const chord_common::TransportLocation &location,
            std::unique_ptr<AbstractAcceptContext> &&ctx);

        void shutdown();

    private:
        StreamManager *m_manager;
        StreamAcceptorOptions m_options;
        AcceptHandle *m_handle;

        friend void new_unix_listener(uv_stream_t *server, int status);
        friend void new_tcp4_listener(uv_stream_t *server, int status);
    };
}

#endif // CHORD_MESH_STREAM_ACCEPTOR_H