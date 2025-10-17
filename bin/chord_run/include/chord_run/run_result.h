#ifndef CHORD_RUN_RUN_RESULT_H
#define CHORD_RUN_RUN_RESULT_H

#include <fmt/core.h>
#include <fmt/format.h>

#include <tempo_utils/log_stream.h>
#include <tempo_utils/status.h>

namespace chord_run {

    constexpr const char *kChordRunStatusNs = "dev.zuri.ns:chord-run-status-1";

    enum class RunCondition {
        kRunInvariant,
    };

    class RunStatus : public tempo_utils::TypedStatus<RunCondition> {
    public:
        using TypedStatus::TypedStatus;
        static bool convert(RunStatus &dstStatus, const tempo_utils::Status &srcStatus);

    private:
        RunStatus(tempo_utils::StatusCode statusCode, std::shared_ptr<const tempo_utils::Detail> detail);

    public:
        /**
         *
         * @param condition
         * @param message
         * @return
         */
        static RunStatus forCondition(
            RunCondition condition,
            std::string_view message)
        {
            return RunStatus(condition, message);
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
        static RunStatus forCondition(
            RunCondition condition,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return RunStatus(condition, message);
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
        static RunStatus forCondition(
            RunCondition condition,
            tempo_utils::TraceId traceId,
            tempo_utils::SpanId spanId,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return RunStatus(condition, message, traceId, spanId);
        }
    };
}

namespace tempo_utils {

    template<>
    struct StatusTraits<chord_run::RunCondition> {
        using ConditionType = chord_run::RunCondition;
        static bool convert(chord_run::RunStatus &dstStatus, const tempo_utils::Status &srcStatus)
        {
            return chord_run::RunStatus::convert(dstStatus, srcStatus);
        }
    };

    template<>
    struct ConditionTraits<chord_run::RunCondition> {
        using StatusType = chord_run::RunStatus;
        static constexpr const char *condition_namespace() { return chord_run::kChordRunStatusNs; }
        static constexpr StatusCode make_status_code(chord_run::RunCondition condition)
        {
            switch (condition) {
                case chord_run::RunCondition::kRunInvariant:
                    return StatusCode::kInternal;
                default:
                    return tempo_utils::StatusCode::kUnknown;
            }
        };
        static constexpr const char *make_error_message(chord_run::RunCondition condition)
        {
            switch (condition) {
                case chord_run::RunCondition::kRunInvariant:
                    return "Run invariant";
                default:
                    return "INVALID";
            }
        }
    };
}

#endif // CHORD_RUN_RUN_RESULT_H