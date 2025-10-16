#ifndef CHORD_SANDBOX_SANDBOX_RESULT_H
#define CHORD_SANDBOX_SANDBOX_RESULT_H

#include <string>

#include <fmt/core.h>
#include <fmt/format.h>

#include <tempo_utils/log_stream.h>
#include <tempo_utils/status.h>

namespace chord_sandbox {

    constexpr const char *kChordSandboxStatusNs("dev.zuri.ns:chord-sandbox-status-1");

    enum class SandboxCondition {
        kAgentError,
        kInvalidConfiguration,
        kInvalidEndpoint,
        kInvalidPlug,
        kInvalidPort,
        kMachineError,
        kSandboxInvariant,
    };


    class SandboxStatus : public tempo_utils::TypedStatus<SandboxCondition> {
    public:
        using TypedStatus::TypedStatus;
        static bool convert(SandboxStatus &dstStatus, const tempo_utils::Status &srcStatus);

    private:
        SandboxStatus(tempo_utils::StatusCode statusCode, std::shared_ptr<const tempo_utils::Detail> detail);

    public:
        /**
         *
         * @param condition
         * @param message
         * @return
         */
        static SandboxStatus forCondition(
            SandboxCondition condition,
            std::string_view message)
        {
            return SandboxStatus(condition, message);
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
        static SandboxStatus forCondition(
            SandboxCondition condition,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return SandboxStatus(condition, message);
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
        static SandboxStatus forCondition(
            SandboxCondition condition,
            tempo_utils::TraceId traceId,
            tempo_utils::SpanId spanId,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return SandboxStatus(condition, message, traceId, spanId);
        }
    };
}

namespace tempo_utils {

    template<>
    struct StatusTraits<chord_sandbox::SandboxCondition> {
        using ConditionType = chord_sandbox::SandboxCondition;
        static bool convert(chord_sandbox::SandboxStatus &dstStatus, const tempo_utils::Status &srcStatus)
        {
            return chord_sandbox::SandboxStatus::convert(dstStatus, srcStatus);
        }
    };

    template<>
    struct ConditionTraits<chord_sandbox::SandboxCondition> {
        using StatusType = chord_sandbox::SandboxStatus;
        static constexpr const char *condition_namespace() { return chord_sandbox::kChordSandboxStatusNs; }
        static constexpr StatusCode make_status_code(chord_sandbox::SandboxCondition condition)
        {
            switch (condition) {
                case chord_sandbox::SandboxCondition::kAgentError:
                case chord_sandbox::SandboxCondition::kInvalidConfiguration:
                case chord_sandbox::SandboxCondition::kInvalidEndpoint:
                case chord_sandbox::SandboxCondition::kInvalidPlug:
                case chord_sandbox::SandboxCondition::kInvalidPort:
                case chord_sandbox::SandboxCondition::kMachineError:
                case chord_sandbox::SandboxCondition::kSandboxInvariant:
                    return tempo_utils::StatusCode::kInternal;
                default:
                    return tempo_utils::StatusCode::kUnknown;
            }
        };
        static constexpr const char *make_error_message(chord_sandbox::SandboxCondition condition)
        {
            switch (condition) {
                case chord_sandbox::SandboxCondition::kAgentError:
                    return "Agent error";
                case chord_sandbox::SandboxCondition::kInvalidConfiguration:
                    return "Invalid configuration";
                case chord_sandbox::SandboxCondition::kInvalidEndpoint:
                    return "Invalid endpoint";
                case chord_sandbox::SandboxCondition::kInvalidPlug:
                    return "Invalid plug";
                case chord_sandbox::SandboxCondition::kInvalidPort:
                    return "Invalid port";
                case chord_sandbox::SandboxCondition::kMachineError:
                    return "Machine error";
                case chord_sandbox::SandboxCondition::kSandboxInvariant:
                    return "Sandbox invariant";
                default:
                    return "INVALID";
            }
        }
    };
}

#endif // CHORD_SANDBOX_SANDBOX_RESULT_H