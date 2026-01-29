#ifndef CHORD_MESH_FLOOD_H
#define CHORD_MESH_FLOOD_H

#include <absl/random/random.h>

#include <chord_common/transport_location.h>

#include "stream.h"
#include "stream_acceptor.h"
#include "stream_connector.h"

namespace chord_mesh {

    struct FloodCallbacks {
        void (*join)(std::string_view, void *) = nullptr;
        void (*leave)(std::string_view, void *) = nullptr;
        void (*receive)(const Envelope &, void *) = nullptr;
        void (*error)(const tempo_utils::Status &, void *) = nullptr;
        void (*cleanup)(void *) = nullptr;
    };

    struct FloodOptions {
        std::vector<chord_common::TransportLocation> initialPeerEndpoints = {};
        std::string protocolName = kDefaultNoiseProtocol;
        bool allowInsecure = false;
        void *data = nullptr;
    };

    class FloodMesh : public std::enable_shared_from_this<FloodMesh> {
    public:
        virtual ~FloodMesh();

        static tempo_utils::Result<std::shared_ptr<FloodMesh>> create(
            const chord_common::TransportLocation &listenEndpoint,
            StreamManager *manager,
            const FloodCallbacks &callbacks,
            const FloodOptions &options = {});


        tempo_utils::Status addPeer(const chord_common::TransportLocation &peerEndpoint);
        tempo_utils::Status removePeer(std::string_view peerId);

        tempo_utils::Status sendMessage(std::string_view peerId, std::shared_ptr<const tempo_utils::ImmutableBytes> payload);
        tempo_utils::Status broadcastMessage(std::shared_ptr<const tempo_utils::ImmutableBytes> payload);

        void shutdown();

    private:

        struct PeerHandle {
            enum class State {
                Waiting,
                Connecting,
                Negotiating,
                Active,
                Closing,
            };
            State state;
            tempo_utils::UUID id;
            std::shared_ptr<Stream> stream;
            std::shared_ptr<FloodMesh> network;
            chord_common::TransportLocation endpoint;
            std::shared_ptr<tempo_security::X509Certificate> certificate;
            absl::Time addedAt;
            absl::Time connectedAt;
            absl::Time lastReceivedAt;
            int connectAttempts;
            uv_timer_t timer;
        };

        struct FloodPriv {
            std::weak_ptr<FloodMesh> network;
            StreamManager *manager;
            std::shared_ptr<StreamAcceptor> acceptor;
            std::shared_ptr<StreamConnector> connector;
            FloodCallbacks callbacks;
            FloodOptions options;
            absl::flat_hash_map<std::string,std::unique_ptr<PeerHandle>> peers;
            absl::BitGen rand;
            bool shutdown;
        };

        std::unique_ptr<FloodPriv> m_priv;

        explicit FloodMesh(std::unique_ptr<FloodPriv> &&priv);

        friend void peer_accepted(std::shared_ptr<Stream> stream, void *data);
        friend void peer_connected(std::shared_ptr<Stream> stream, void *data);
        friend bool peer_negotiate(std::string_view protocolName, std::shared_ptr<tempo_security::X509Certificate> certificate, void *data);
        friend void peer_receive(const Envelope &message, void *data);
        friend void accept_error(const tempo_utils::Status &status, void *data);
        friend void connect_error(const tempo_utils::Status &status, void *data);
        friend void retry_connect(uv_timer_t *handle);
    };
}

#endif // CHORD_MESH_FLOOD_H