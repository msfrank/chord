#ifndef CHORD_MACHINE_MACHINE_RESULT_H
#define CHORD_MACHINE_MACHINE_RESULT_H

#include <string>

#include <fmt/core.h>
#include <fmt/format.h>

#include <tempo_utils/status.h>

namespace chord_machine {

    constexpr const char *kChordMachineStatusNs("dev.zuri.ns:chord-machine-status-1");

    enum class MachineCondition {
        kInvalidConfiguration,
        kMachineInvariant,
    };


    class MachineStatus : public tempo_utils::TypedStatus<MachineCondition> {
    public:
        using TypedStatus::TypedStatus;
        static bool convert(MachineStatus &dstStatus, const tempo_utils::Status &srcStatus);

    private:
        MachineStatus(tempo_utils::StatusCode statusCode, std::shared_ptr<const tempo_utils::Detail> detail);

    public:
        /**
         *
         * @param condition
         * @param message
         * @return
         */
        static MachineStatus forCondition(
            MachineCondition condition,
            std::string_view message)
        {
            return MachineStatus(condition, message);
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
        static MachineStatus forCondition(
            MachineCondition condition,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return MachineStatus(condition, message);
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
        static MachineStatus forCondition(
            MachineCondition condition,
            tempo_utils::TraceId traceId,
            tempo_utils::SpanId spanId,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return MachineStatus(condition, message, traceId, spanId);
        }
    };
}

namespace tempo_utils {

    template<>
    struct StatusTraits<chord_machine::MachineCondition> {
        using ConditionType = chord_machine::MachineCondition;
        static bool convert(chord_machine::MachineStatus &dstStatus, const tempo_utils::Status &srcStatus)
        {
            return chord_machine::MachineStatus::convert(dstStatus, srcStatus);
        }
    };

    template<>
    struct ConditionTraits<chord_machine::MachineCondition> {
        using StatusType = chord_machine::MachineStatus;
        static constexpr const char *condition_namespace() { return chord_machine::kChordMachineStatusNs; }
        static constexpr StatusCode make_status_code(chord_machine::MachineCondition condition)
        {
            switch (condition) {
                case chord_machine::MachineCondition::kInvalidConfiguration:
                case chord_machine::MachineCondition::kMachineInvariant:
                    return tempo_utils::StatusCode::kInternal;
                default:
                    return tempo_utils::StatusCode::kUnknown;
            }
        };
        static constexpr const char *make_error_message(chord_machine::MachineCondition condition)
        {
            switch (condition) {
                case chord_machine::MachineCondition::kInvalidConfiguration:
                    return "Invalid configuration";
                case chord_machine::MachineCondition::kMachineInvariant:
                    return "Machine invariant";
                default:
                    return "INVALID";
            }
        }
    };
}

#endif // CHORD_MACHINE_MACHINE_RESULT_H