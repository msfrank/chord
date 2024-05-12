
#include <chord_test/test_result.h>

chord_test::TestStatus::TestStatus(
    tempo_utils::StatusCode statusCode,
    std::shared_ptr<const tempo_utils::Detail> detail)
    : tempo_utils::TypedStatus<TestCondition>(statusCode, detail)
{
}

chord_test::TestStatus
chord_test::TestStatus::ok()
{
    return TestStatus();
}

bool
chord_test::TestStatus::convert(TestStatus &dstStatus, const tempo_utils::Status &srcStatus)
{
    std::string_view srcNs = srcStatus.getErrorCategory();
    std::string_view dstNs = kChordTestStatusNs.getNs();
    if (srcNs != dstNs)
        return false;
    dstStatus = TestStatus(srcStatus.getStatusCode(), srcStatus.getDetail());
    return true;
}

chord_test::TestException::TestException(const TestStatus &status) noexcept
    : m_status(status)
{
}

chord_test::TestStatus
chord_test::TestException::getStatus() const
{
    return m_status;
}

const char *
chord_test::TestException::what() const noexcept
{
    return m_status.getMessage().data();
}