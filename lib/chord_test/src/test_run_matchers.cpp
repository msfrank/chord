
#include <chord_test/test_run_matchers.h>

chord_test::matchers::RunMachineMatcher::RunMachineMatcher(tempo_utils::StatusCode statusCode)
    : m_statusCode(statusCode)
{
}

bool
chord_test::matchers::RunMachineMatcher::MatchAndExplain(
    const chord_test::RunMachine &runMachine,
    std::ostream *os) const
{
    auto exit = runMachine.getMachineExit();
    return exit.statusCode == m_statusCode;
}

void
chord_test::matchers::RunMachineMatcher::DescribeTo(std::ostream *os) const
{
    *os << "machine exit status code matches " << tempo_utils::status_code_to_string(m_statusCode);
}

void
chord_test::matchers::RunMachineMatcher::DescribeNegationTo(std::ostream *os) const
{
    *os << "machine exit status code does not match " << tempo_utils::status_code_to_string(m_statusCode);
}

::testing::Matcher<chord_test::RunMachine>
chord_test::matchers::RunMachine(tempo_utils::StatusCode statusCode)
{
    return RunMachineMatcher(statusCode);
}
