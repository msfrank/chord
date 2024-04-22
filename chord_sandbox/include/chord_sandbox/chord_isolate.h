#ifndef CHORD_SANDBOX_CHORD_SANDBOX_H
#define CHORD_SANDBOX_CHORD_SANDBOX_H

#include <filesystem>
#include <vector>

#include <grpcpp/channel.h>

#include <chord_protocol/protocol_types.h>
#include <chord_sandbox/remote_machine.h>
#include <chord_sandbox/sandbox_result.h>
#include <chord_sandbox/sandbox_types.h>
#include <lyric_common/assembly_location.h>
#include <tempo_config/config_types.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_utils/url.h>

namespace chord_sandbox {

    enum class AgentDiscoveryPolicy {
        USE_SPECIFIED_ENDPOINT,
        SPAWN_IF_MISSING,
        ALWAYS_SPAWN,
    };

    struct SandboxOptions {
        AgentDiscoveryPolicy discoveryPolicy;
        tempo_utils::Url agentEndpoint;
        chord_protocol::TransportType endpointTransport;
        std::string agentServerName;
        std::filesystem::path agentPath;
        std::filesystem::path runDirectory;
        tempo_security::CertificateKeyPair caKeyPair;
        tempo_security::CertificateKeyPair agentKeyPair;
        std::filesystem::path pemRootCABundleFile;
    };

    struct SandboxPriv;

    class ChordIsolate {

    public:
        ChordIsolate();
        explicit ChordIsolate(const SandboxOptions &options = {});
        ~ChordIsolate();

        tempo_utils::Status initialize();

        tempo_utils::Result<std::shared_ptr<RemoteMachine>> spawn(
            std::string_view name,
            const lyric_common::AssemblyLocation &mainLocation,
            const tempo_config::ConfigMap &configMap,
            const absl::flat_hash_set<chord_protocol::RequestedPort> &requestedPorts,
            const absl::flat_hash_map<tempo_utils::Url,std::shared_ptr<chord_protocol::AbstractProtocolHandler>> &plugs,
            RunProtocolCallback cb,
            void *cbData);

        tempo_utils::Status shutdown();

    private:
        SandboxOptions m_options;
        std::shared_ptr<grpc::Channel> m_channel;
        std::unique_ptr<SandboxPriv> m_priv;
        absl::flat_hash_map<tempo_utils::Url,std::shared_ptr<RemoteMachine>> m_machines;
    };
}

#endif // CHORD_SANDBOX_CHORD_SANDBOX_H