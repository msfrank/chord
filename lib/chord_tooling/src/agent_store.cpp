
#include <chord_tooling/agent_store.h>
#include <chord_tooling/tooling_conversions.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/container_conversions.h>
#include <tempo_config/parse_config.h>

chord_tooling::AgentStore::AgentStore(
    const absl::flat_hash_map<std::string,std::shared_ptr<const AgentEntry>> &agentEntries)
    : m_agentEntries(agentEntries)
{
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

tempo_utils::Status
chord_tooling::AgentStore::putAgent(std::string_view agentName, std::shared_ptr<const AgentEntry> agentEntry)
{
    m_agentEntries[agentName] = std::move(agentEntry);
    return {};
}

tempo_utils::Status
chord_tooling::AgentStore::removeAgent(std::string_view agentName)
{
    m_agentEntries.erase(agentName);
    return {};
}