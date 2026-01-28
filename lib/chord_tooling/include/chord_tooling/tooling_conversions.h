#ifndef CHORD_TOOLING_TOOLING_CONVERSIONS_H
#define CHORD_TOOLING_TOOLING_CONVERSIONS_H

#include <tempo_config/abstract_converter.h>

#include "agent_store.h"
#include "security_config.h"
#include "signer_store.h"

namespace chord_tooling {

    class SignerEntryParser : public tempo_config::AbstractConverter<SignerEntry> {
    public:
        tempo_utils::Status parseLocalSigner(const tempo_config::ConfigMap &map, SignerEntry &signerEntry) const;

        tempo_utils::Status convertValue(
            const tempo_config::ConfigNode &node,
            SignerEntry &signerEntry) const override;
    };

    class SignerStoreParser : public tempo_config::AbstractConverter<SignerStore> {
    public:
        tempo_utils::Status convertValue(
            const tempo_config::ConfigNode &node,
            SignerStore &signerStore) const override;
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