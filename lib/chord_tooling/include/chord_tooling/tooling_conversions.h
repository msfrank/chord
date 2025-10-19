#ifndef CHORD_TOOLING_TOOLING_CONVERSIONS_H
#define CHORD_TOOLING_TOOLING_CONVERSIONS_H

#include <tempo_config/abstract_converter.h>

#include "agent_store.h"
#include "security_config.h"

namespace chord_tooling {

    class SecurityConfigParser : public tempo_config::AbstractConverter<SecurityConfig> {
    public:
        tempo_utils::Status convertValue(
            const tempo_config::ConfigNode &node,
            SecurityConfig &securityConfig) const override;
    };

    class AgentEntryParser : public tempo_config::AbstractConverter<AgentEntry> {
    public:
        tempo_utils::Status parseUnixTransport(const tempo_config::ConfigMap &map, AgentEntry &agentEntry) const;
        tempo_utils::Status parseTcp4Transport(const tempo_config::ConfigMap &map, AgentEntry &agentEntry) const;

        tempo_utils::Status convertValue(
            const tempo_config::ConfigNode &node,
            AgentEntry &agentEntry) const override;
    };

    class AgentStoreParser : public tempo_config::AbstractConverter<AgentStore> {
    public:
        tempo_utils::Status convertValue(
            const tempo_config::ConfigNode &node,
            AgentStore &agentStore) const override;
    };
}

#endif // CHORD_TOOLING_TOOLING_CONVERSIONS_H