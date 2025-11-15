#ifndef CHORD_MESH_STREAM_ACCEPTOR_H
#define CHORD_MESH_STREAM_ACCEPTOR_H

#include <uv.h>

#include <chord_common/transport_location.h>
#include <tempo_utils/result.h>

#include "stream.h"

namespace chord_mesh {

    enum class AcceptorState {
        Initial,
        Active,
        Closed,
    };

    struct StreamAcceptorOps {
        void (*accept)(std::shared_ptr<Stream>, void *) = nullptr;
        void (*error)(const tempo_utils::Status &, void *) = nullptr;
        void (*cleanup)(void *) = nullptr;
    };

    struct StreamAcceptorOptions {
        bool allowInsecure = false;
        void *data = nullptr;
    };

    class StreamAcceptor {
    public:
        virtual ~StreamAcceptor();

        static tempo_utils::Result<std::shared_ptr<StreamAcceptor>> forUnix(
            std::string_view pipePath,
            int pipeFlags,
            StreamManager *manager);
        static tempo_utils::Result<std::shared_ptr<StreamAcceptor>> forTcp4(
            std::string_view ipAddress,
            tu_uint16 tcpPort,
            StreamManager *manager);
        static tempo_utils::Result<std::shared_ptr<StreamAcceptor>> forLocation(
            const chord_common::TransportLocation &endpoint,
            StreamManager *manager);

        AcceptorState getAcceptorState() const;

        tempo_utils::Status listen(const StreamAcceptorOps &ops, const StreamAcceptorOptions &options = {});
        void shutdown();

    private:
        StreamHandle *m_handle;

        AcceptorState m_state;
        StreamAcceptorOps m_ops;
        StreamAcceptorOptions m_options;

        explicit StreamAcceptor(StreamHandle *handle);
        void emitError(const tempo_utils::Status &status);

        friend void new_unix_listener(uv_stream_t *server, int status);
        friend void new_tcp4_listener(uv_stream_t *server, int status);
    };
}

#endif // CHORD_MESH_STREAM_ACCEPTOR_H