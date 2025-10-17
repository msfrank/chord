#ifndef CHORD_TOOLING_TOOLING_RESULT_H
#define CHORD_TOOLING_TOOLING_RESULT_H

#include <fmt/core.h>
#include <fmt/format.h>

#include <tempo_utils/log_stream.h>
#include <tempo_utils/status.h>

namespace chord_tooling {

    constexpr const char *kChordToolingStatusNs = "dev.zuri.ns:chord-tooling-status-1";

    enum class ToolingCondition {
        kToolingInvariant,
    };

    class ToolingStatus : public tempo_utils::TypedStatus<ToolingCondition> {
    public:
        using TypedStatus::TypedStatus;
        static bool convert(ToolingStatus &dstStatus, const tempo_utils::Status &srcStatus);

    private:
        ToolingStatus(tempo_utils::StatusCode statusCode, std::shared_ptr<const tempo_utils::Detail> detail);

    public:
        /**
         *
         * @param condition
         * @param message
         * @return
         */
        static ToolingStatus forCondition(
            ToolingCondition condition,
            std::string_view message)
        {
            return ToolingStatus(condition, message);
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
        static ToolingStatus forCondition(
            ToolingCondition condition,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return ToolingStatus(condition, message);
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
        static ToolingStatus forCondition(
            ToolingCondition condition,
            tempo_utils::TraceId traceId,
            tempo_utils::SpanId spanId,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return ToolingStatus(condition, message, traceId, spanId);
        }
    };
}

namespace tempo_utils {

    template<>
    struct StatusTraits<chord_tooling::ToolingCondition> {
        using ConditionType = chord_tooling::ToolingCondition;
        static bool convert(chord_tooling::ToolingStatus &dstStatus, const tempo_utils::Status &srcStatus)
        {
            return chord_tooling::ToolingStatus::convert(dstStatus, srcStatus);
        }
    };

    template<>
    struct ConditionTraits<chord_tooling::ToolingCondition> {
        using StatusType = chord_tooling::ToolingStatus;
        static constexpr const char *condition_namespace() { return chord_tooling::kChordToolingStatusNs; }
        static constexpr StatusCode make_status_code(chord_tooling::ToolingCondition condition)
        {
            switch (condition) {
                case chord_tooling::ToolingCondition::kToolingInvariant:
                    return StatusCode::kInternal;
                default:
                    return tempo_utils::StatusCode::kUnknown;
            }
        };
        static constexpr const char *make_error_message(chord_tooling::ToolingCondition condition)
        {
            switch (condition) {
                case chord_tooling::ToolingCondition::kToolingInvariant:
                    return "Tooling invariant";
                default:
                    return "INVALID";
            }
        }
    };
}

#endif // CHORD_TOOLING_TOOLING_RESULT_H