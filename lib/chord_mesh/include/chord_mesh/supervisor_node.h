#ifndef CHORD_MESH_SUPERVISOR_NODE_H
#define CHORD_MESH_SUPERVISOR_NODE_H

#include <uv.h>
#include <nng/nng.h>

#include <chord_common/transport_location.h>
#include <tempo_utils/result.h>

#include "stream.h"
#include "stream_acceptor.h"
#include "stream_connector.h"

namespace chord_mesh {

    class SupervisorNode;

    struct SupervisorPeer {
        std::shared_ptr<Stream> stream;
        SupervisorNode *supervisor;
        absl::Time connectedAt;
        chord_common::TransportLocation endpoint;
        std::string certificate;
        absl::Time lastMessageAt;
    };

    struct ConnectingPeer {
        chord_common::TransportLocation endpoint;
        SupervisorNode *supervisor;
        absl::Time startedAt;
    };

    struct SupervisorNodeOptions {
        std::vector<chord_common::TransportLocation> initialPeerEndpoints = {};
    };

    class SupervisorNode {
    public:
        virtual ~SupervisorNode();

        static tempo_utils::Result<std::shared_ptr<SupervisorNode>> create(
            const chord_common::TransportLocation &supervisorEndpoint,
            const tempo_security::CertificateKeyPair &keypair,
            StreamManager *manager,
            const SupervisorNodeOptions &options = {});

        tempo_utils::Status start();
        tempo_utils::Status connect(const chord_common::TransportLocation &endpoint);
        tempo_utils::Status shutdown();

    private:
        chord_common::TransportLocation m_supervisorEndpoint;
        tempo_security::CertificateKeyPair m_keypair;
        StreamManager *m_manager;
        SupervisorNodeOptions m_options;

        std::shared_ptr<StreamAcceptor> m_acceptor;
        absl::flat_hash_map<tempo_utils::UUID,std::shared_ptr<SupervisorPeer>> m_peers;
        absl::flat_hash_map<chord_common::TransportLocation,std::unique_ptr<ConnectingPeer>> m_connecting;
        absl::flat_hash_set<std::shared_ptr<SupervisorPeer>> m_lazy;
        absl::flat_hash_set<std::shared_ptr<SupervisorPeer>> m_eager;
        std::unique_ptr<StreamConnector> m_connector;

        SupervisorNode(
            const chord_common::TransportLocation &supervisorEndpoint,
            const tempo_security::CertificateKeyPair &keypair,
            StreamManager *manager,
            std::shared_ptr<StreamAcceptor> acceptor,
            const SupervisorNodeOptions &options);

        friend void on_supervisor_stream_receive(const Message &message, void *data);
        friend void on_supervisor_acceptor_accept(std::shared_ptr<Stream> stream, void *data);
        friend void on_supervisor_acceptor_error(const tempo_utils::Status &status, void *data);
        friend void on_supervisor_acceptor_cleanup(void *data);
        friend void on_supervisor_connector_connect(std::shared_ptr<Stream> stream, void *data);
        friend void on_supervisor_connector_error(const tempo_utils::Status &status, void *data);
        friend void on_supervisor_connector_cleanup(void *data);
    };
}

#endif // CHORD_MESH_SUPERVISOR_NODE_H