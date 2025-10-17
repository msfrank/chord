
#include <chord_tooling/agent_store.h>
#include <chord_tooling/tooling_conversions.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/container_conversions.h>
#include <tempo_config/parse_config.h>

chord_tooling::AgentStore::AgentStore(const tempo_config::ConfigMap &agentsMap)
    : m_agentsMap(agentsMap)
{
}

tempo_utils::Status
chord_tooling::AgentStore::configure()
{
    tempo_config::StringParser agentNameParser;
    AgentEntryParser agentEntryParser;
    tempo_config::SharedPtrConstTParser sharedConstTargetEntryParser(&agentEntryParser);
    tempo_config::MapKVParser agentEntriesParser(&agentNameParser, &sharedConstTargetEntryParser);

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(m_agentEntries, agentEntriesParser, m_agentsMap));

    return {};
}

bool
chord_tooling::AgentStore::hasAgent(std::string_view agentName) const
{
    return m_agentEntries.contains(agentName);
}

std::shared_ptr<const chord_tooling::AgentEntry>
chord_tooling::AgentStore::getAgent(std::string_view agentName) const
{
    auto entry = m_agentEntries.find(agentName);
    if (entry != m_agentEntries.cend())
        return entry->second;
    return {};
}

absl::flat_hash_map<std::string,std::shared_ptr<const chord_tooling::AgentEntry>>::const_iterator
chord_tooling::AgentStore::agentsBegin() const
{
    return m_agentEntries.cbegin();
}

absl::flat_hash_map<std::string,std::shared_ptr<const chord_tooling::AgentEntry>>::const_iterator
chord_tooling::AgentStore::agentsEnd() const
{
    return m_agentEntries.cend();
}

int
chord_tooling::AgentStore::numAgents() const
{
    return m_agentEntries.size();
}