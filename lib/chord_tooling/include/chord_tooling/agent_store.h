#ifndef CHORD_TOOLING_AGENT_STORE_H
#define CHORD_TOOLING_AGENT_STORE_H

#include <chord_common/transport_location.h>
#include <tempo_config/config_types.h>
#include <tempo_utils/status.h>

namespace chord_tooling {

    struct AgentEntry {
        chord_common::TransportLocation agentLocation;
        std::vector<std::filesystem::path> packageCacheDirectories;
        std::filesystem::path pemCertificateFile;
        std::filesystem::path pemPrivateKeyFile;
        absl::Duration idleTimeout;
        absl::Duration registrationTimeout;
    };

    class AgentStore {
    public:
        explicit AgentStore(const tempo_config::ConfigMap &agentsMap);

        tempo_utils::Status configure();

        bool hasAgent(std::string_view agentName) const;
        std::shared_ptr<const AgentEntry> getAgent(std::string_view agentName) const;
        absl::flat_hash_map<std::string,std::shared_ptr<const AgentEntry>>::const_iterator agentsBegin() const;
        absl::flat_hash_map<std::string,std::shared_ptr<const AgentEntry>>::const_iterator agentsEnd() const;
        int numAgents() const;

    private:
        tempo_config::ConfigMap m_agentsMap;

        absl::flat_hash_map<std::string,std::shared_ptr<const AgentEntry>> m_agentEntries;
    };
}

#endif // CHORD_TOOLING_AGENT_STORE_H