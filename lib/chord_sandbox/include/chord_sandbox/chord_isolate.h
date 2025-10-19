#ifndef CHORD_SANDBOX_CHORD_SANDBOX_H
#define CHORD_SANDBOX_CHORD_SANDBOX_H

#include <filesystem>
#include <vector>

#include <grpcpp/channel.h>

#include <chord_common/protocol_types.h>
#include <chord_common/transport_location.h>
#include <chord_sandbox/remote_machine.h>
#include <chord_sandbox/sandbox_result.h>
#include <chord_sandbox/sandbox_types.h>
#include <lyric_common/module_location.h>
#include <tempo_config/config_types.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_utils/url.h>

#include "abstract_endpoint_signer.h"
#include "chord_tooling/chord_config.h"

namespace chord_sandbox {

    struct IsolateOptions {
        std::filesystem::path agentPath = {};
        std::string agentServerNameOverride = {};
        std::filesystem::path runDirectory = {};
        std::shared_ptr<AbstractEndpointSigner> endpointSigner = {};
    };

    struct RequestedPortAndHandler {
        chord_common::RequestedPort requestedPort;
        std::shared_ptr<chord_common::AbstractProtocolHandler> handler;
    };

    /**
     *
     */
    class ChordIsolate {

    public:
        virtual ~ChordIsolate() = default;

        static tempo_utils::Result<std::shared_ptr<ChordIsolate>> spawn(
            std::string_view agentName,
            std::shared_ptr<const chord_tooling::SecurityConfig> securityConfig,
            std::shared_ptr<const chord_tooling::AgentEntry> agentEntry,
            const IsolateOptions &options = {});

        static tempo_utils::Result<std::shared_ptr<ChordIsolate>> spawn(
            std::string_view agentName,
            std::shared_ptr<const chord_tooling::ChordConfig> chordConfig,
            const IsolateOptions &options = {});

        static tempo_utils::Result<std::shared_ptr<ChordIsolate>> connect(
            std::string_view agentName,
            const chord_common::TransportLocation &agentEndpoint,
            std::shared_ptr<const chord_tooling::SecurityConfig> securityConfig,
            const IsolateOptions &options = {});

        static tempo_utils::Result<std::shared_ptr<ChordIsolate>> connect(
            std::string_view agentName,
            std::shared_ptr<const chord_tooling::ChordConfig> chordConfig,
            const IsolateOptions &options = {});

        tempo_utils::Result<std::shared_ptr<RemoteMachine>> launch(
            std::string_view name,
            const tempo_utils::Url &mainLocation,
            const tempo_config::ConfigMap &configMap,
            const std::vector<RequestedPortAndHandler> &plugs = {},
            bool startSuspended = false);

        tempo_utils::Status shutdown();

    private:
        std::string m_name;
        std::filesystem::path m_pemRootCABundleFile;
        std::shared_ptr<AbstractEndpointSigner> m_endpointSigner;
        std::shared_ptr<grpc::Channel> m_channel;

        struct SandboxPriv;
        std::unique_ptr<SandboxPriv> m_priv;

        absl::flat_hash_map<tempo_utils::Url,std::shared_ptr<RemoteMachine>> m_machines;

        ChordIsolate(
            std::string_view agentName,
            const std::filesystem::path &pemRootCABundleFile,
            std::shared_ptr<AbstractEndpointSigner> endpointSigner,
            std::shared_ptr<grpc::Channel> channel,
            std::unique_ptr<SandboxPriv> priv);
    };
}

#endif // CHORD_SANDBOX_CHORD_SANDBOX_H
