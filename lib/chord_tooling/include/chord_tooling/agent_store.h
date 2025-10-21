#ifndef CHORD_TOOLING_AGENT_STORE_H
#define CHORD_TOOLING_AGENT_STORE_H

#include <chord_common/transport_location.h>
#include <tempo_config/config_types.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_utils/status.h>

namespace chord_tooling {

    struct AgentEntry {
        chord_common::TransportType transportType;
        std::filesystem::path unixListenerPath;
        std::string tcpListenerAddress;
        tu_uint16 tcpListenerPort;
        std::vector<std::filesystem::path> packageCacheDirectories;
        std::string certificateSigner;
        absl::Duration idleTimeout;
        absl::Duration registrationTimeout;

        tempo_utils::Result<tempo_security::CertificateKeyPair> getAgentKeyPair() const;
    };

    class AgentStore {
    public:
        explicit AgentStore(
            const absl::flat_hash_map<std::string,std::shared_ptr<const AgentEntry>> &agentEntries = {});

        bool hasAgent(std::string_view agentName) const;
        std::shared_ptr<const AgentEntry> getAgent(std::string_view agentName) const;
        absl::flat_hash_map<std::string,std::shared_ptr<const AgentEntry>>::const_iterator agentsBegin() const;
        absl::flat_hash_map<std::string,std::shared_ptr<const AgentEntry>>::const_iterator agentsEnd() const;
        int numAgents() const;

        tempo_utils::Status putAgent(std::string_view agentName, std::shared_ptr<const AgentEntry> agentEntry);
        tempo_utils::Status removeAgent(std::string_view agentName);

    private:
        absl::flat_hash_map<std::string,std::shared_ptr<const AgentEntry>> m_agentEntries;
    };
}

#endif // CHORD_TOOLING_AGENT_STORE_H