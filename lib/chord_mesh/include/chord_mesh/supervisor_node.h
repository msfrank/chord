#ifndef CHORD_MESH_SUPERVISOR_NODE_H
#define CHORD_MESH_SUPERVISOR_NODE_H

#include <uv.h>
#include <nng/nng.h>

#include <tempo_utils/result.h>

#include "stream_connector.h"

namespace chord_mesh {

    struct Peer {
        enum class Type {
            Invalid,
            User,
            Idp,
            Supervisor,
            Machine,
        };
        Type type;
        absl::Time firstSeen;
        absl::Time lastSeen;
        bool hasCertificate;
    };

    struct SupervisorNodeOptions {
        std::vector<std::string> initialPeerUriList = {};
    };

    class SupervisorNode {
    public:
        virtual ~SupervisorNode() = default;

        static tempo_utils::Result<std::shared_ptr<SupervisorNode>> create(
            const std::string &ensembleEventsUri,
            uv_loop_t *loop,
            const SupervisorNodeOptions &options = {});

        tempo_utils::Status configure();

    private:
        std::string m_ensembleEventsUri;
        uv_loop_t *m_loop;

        //std::unique_ptr<SocketPoller> m_ensembleEvents;

        SupervisorNode(
            const std::string &ensembleEventsUri,
            uv_loop_t *loop);

        friend void ensemble_events_sendmsg(void *data);
        friend void ensemble_events_recvmsg(nng_msg *message, void *data);
    };
}

#endif // CHORD_MESH_SUPERVISOR_NODE_H