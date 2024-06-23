#ifndef CHORD_TEST_TEST_RUN_MATCHERS_H
#define CHORD_TEST_TEST_RUN_MATCHERS_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "test_run.h"

namespace chord_test::matchers {

    class SpawnMachineMatcher {

    public:
        explicit SpawnMachineMatcher(tempo_utils::StatusCode statusCode);

        bool MatchAndExplain(const chord_test::SpawnMachine &runMachine, std::ostream *os) const;
        void DescribeTo(std::ostream *os) const;
        void DescribeNegationTo(std::ostream *os) const;

        using MatchesType = chord_test::SpawnMachine;
        using is_gtest_matcher = void;

    private:
        tempo_utils::StatusCode m_statusCode;
    };

    ::testing::Matcher<SpawnMachine> SpawnMachine(tempo_utils::StatusCode statusCode);

    class RunMachineMatcher {

    public:
        explicit RunMachineMatcher(tempo_utils::StatusCode statusCode);

        bool MatchAndExplain(const chord_test::RunMachine &runMachine, std::ostream *os) const;
        void DescribeTo(std::ostream *os) const;
        void DescribeNegationTo(std::ostream *os) const;

        using MatchesType = chord_test::RunMachine;
        using is_gtest_matcher = void;

    private:
        tempo_utils::StatusCode m_statusCode;
    };

    ::testing::Matcher<RunMachine> RunMachine(tempo_utils::StatusCode statusCode);
}

#endif // CHORD_TEST_TEST_RUN_MATCHERS_H
