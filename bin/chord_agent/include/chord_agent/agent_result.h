#ifndef CHORD_AGENT_AGENT_RESULT_H
#define CHORD_AGENT_AGENT_RESULT_H

#include <string>

#include <fmt/core.h>
#include <fmt/format.h>

#include <tempo_utils/status.h>

namespace chord_agent {

    constexpr const char *kChordAgentStatusNs("dev.zuri.ns:chord-agent-status-1");

    enum class AgentCondition {
        kInvalidConfiguration,
        kAgentInvariant,
    };


    class AgentStatus : public tempo_utils::TypedStatus<AgentCondition> {
    public:
        using TypedStatus::TypedStatus;
        static bool convert(AgentStatus &dstStatus, const tempo_utils::Status &srcStatus);

    private:
        AgentStatus(tempo_utils::StatusCode statusCode, std::shared_ptr<const tempo_utils::Detail> detail);

    public:
        /**
         *
         * @param condition
         * @param message
         * @return
         */
        static AgentStatus forCondition(
            AgentCondition condition,
            std::string_view message)
        {
            return AgentStatus(condition, message);
        }
        /**
         *
         * @tparam Args
         * @param condition
         * @param messageFmt
         * @param messageArgs
         * @return
         */
        template <typename... Args>
        static AgentStatus forCondition(
            AgentCondition condition,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return AgentStatus(condition, message);
        }
        /**
         *
         * @tparam Args
         * @param condition
         * @param messageFmt
         * @param messageArgs
         * @return
         */
        template <typename... Args>
        static AgentStatus forCondition(
            AgentCondition condition,
            tempo_utils::TraceId traceId,
            tempo_utils::SpanId spanId,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return AgentStatus(condition, message, traceId, spanId);
        }
    };
}

namespace tempo_utils {

    template<>
    struct StatusTraits<chord_agent::AgentCondition> {
        using ConditionType = chord_agent::AgentCondition;
        static bool convert(chord_agent::AgentStatus &dstStatus, const tempo_utils::Status &srcStatus)
        {
            return chord_agent::AgentStatus::convert(dstStatus, srcStatus);
        }
    };

    template<>
    struct ConditionTraits<chord_agent::AgentCondition> {
        using StatusType = chord_agent::AgentStatus;
        static constexpr const char *condition_namespace() { return chord_agent::kChordAgentStatusNs; }
        static constexpr StatusCode make_status_code(chord_agent::AgentCondition condition)
        {
            switch (condition) {
                case chord_agent::AgentCondition::kInvalidConfiguration:
                case chord_agent::AgentCondition::kAgentInvariant:
                    return tempo_utils::StatusCode::kInternal;
                default:
                    return tempo_utils::StatusCode::kUnknown;
            }
        };
        static constexpr const char *make_error_message(chord_agent::AgentCondition condition)
        {
            switch (condition) {
                case chord_agent::AgentCondition::kInvalidConfiguration:
                    return "Invalid configuration";
                case chord_agent::AgentCondition::kAgentInvariant:
                    return "Agent invariant";
                default:
                    return "INVALID";
            }
        }
    };
}

#endif // CHORD_AGENT_AGENT_RESULT_H