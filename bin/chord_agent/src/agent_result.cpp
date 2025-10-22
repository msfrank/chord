
#include <chord_agent/agent_result.h>

chord_agent::AgentStatus::AgentStatus(
    tempo_utils::StatusCode statusCode,
    std::shared_ptr<const tempo_utils::Detail> detail)
    : tempo_utils::TypedStatus<AgentCondition>(statusCode, detail)
{
}

bool
chord_agent::AgentStatus::convert(AgentStatus &dstStatus, const tempo_utils::Status &srcStatus)
{
    std::string_view srcNs = srcStatus.getErrorCategory();
    std::string_view dstNs = kChordAgentStatusNs;
    if (srcNs != dstNs)
        return false;
    dstStatus = AgentStatus(srcStatus.getStatusCode(), srcStatus.getDetail());
    return true;
}
