#ifndef CHORD_TEST_TEST_RESULT_H
#define CHORD_TEST_TEST_RESULT_H

#include <string>

#include <fmt/core.h>
#include <fmt/format.h>

#include <tempo_utils/status.h>

namespace chord_test {

    constexpr const char *kChordTestStatusNs("dev.zuri.ns:chord-test-status-1");

    enum class TestCondition {
        kInvalidConfiguration,
        kTestInvariant,
    };


    class TestStatus : public tempo_utils::TypedStatus<TestCondition> {
    public:
        using TypedStatus::TypedStatus;
        static bool convert(TestStatus &dstStatus, const tempo_utils::Status &srcStatus);

    private:
        TestStatus(tempo_utils::StatusCode statusCode, std::shared_ptr<const tempo_utils::Detail> detail);

    public:
        /**
         *
         * @param condition
         * @param message
         * @return
         */
        static TestStatus forCondition(
            TestCondition condition,
            std::string_view message)
        {
            return TestStatus(condition, message);
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
        static TestStatus forCondition(
            TestCondition condition,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return TestStatus(condition, message);
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
        static TestStatus forCondition(
            TestCondition condition,
            tempo_utils::TraceId traceId,
            tempo_utils::SpanId spanId,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return TestStatus(condition, message, traceId, spanId);
        }
    };
}

namespace tempo_utils {

    template<>
    struct StatusTraits<chord_test::TestCondition> {
        using ConditionType = chord_test::TestCondition;
        static bool convert(chord_test::TestStatus &dstStatus, const tempo_utils::Status &srcStatus)
        {
            return chord_test::TestStatus::convert(dstStatus, srcStatus);
        }
    };

    template<>
    struct ConditionTraits<chord_test::TestCondition> {
        using StatusType = chord_test::TestStatus;
        static constexpr const char *condition_namespace() { return chord_test::kChordTestStatusNs; }
        static constexpr StatusCode make_status_code(chord_test::TestCondition condition)
        {
            switch (condition) {
                case chord_test::TestCondition::kInvalidConfiguration:
                case chord_test::TestCondition::kTestInvariant:
                    return tempo_utils::StatusCode::kInternal;
                default:
                    return tempo_utils::StatusCode::kUnknown;
            }
        };
        static constexpr const char *make_error_message(chord_test::TestCondition condition)
        {
            switch (condition) {
                case chord_test::TestCondition::kInvalidConfiguration:
                    return "Invalid configuration";
                case chord_test::TestCondition::kTestInvariant:
                    return "Test invariant";
                default:
                    return "INVALID";
            }
        }
    };
}

#endif // CHORD_TEST_TEST_RESULT_H