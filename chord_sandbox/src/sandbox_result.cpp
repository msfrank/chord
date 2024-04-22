
#include <chord_sandbox/sandbox_result.h>

chord_sandbox::SandboxStatus::SandboxStatus(
    tempo_utils::StatusCode statusCode,
    std::shared_ptr<const tempo_utils::Detail> detail)
    : tempo_utils::TypedStatus<SandboxCondition>(statusCode, detail)
{
}

chord_sandbox::SandboxStatus
chord_sandbox::SandboxStatus::ok()
{
    return SandboxStatus();
}

bool
chord_sandbox::SandboxStatus::convert(SandboxStatus &dstStatus, const tempo_utils::Status &srcStatus)
{
    std::string_view srcNs = srcStatus.getErrorCategory();
    std::string_view dstNs = kLyricSandboxStatusNs.getNs();
    if (srcNs != dstNs)
        return false;
    dstStatus = SandboxStatus(srcStatus.getStatusCode(), srcStatus.getDetail());
    return true;
}

chord_sandbox::SandboxException::SandboxException(const SandboxStatus &status) noexcept
    : m_status(status)
{
}

chord_sandbox::SandboxStatus
chord_sandbox::SandboxException::getStatus() const
{
    return m_status;
}

const char *
chord_sandbox::SandboxException::what() const noexcept
{
    return m_status.getMessage().data();
}