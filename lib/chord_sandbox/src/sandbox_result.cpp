
#include <chord_sandbox/sandbox_result.h>

chord_sandbox::SandboxStatus::SandboxStatus(
    tempo_utils::StatusCode statusCode,
    std::shared_ptr<const tempo_utils::Detail> detail)
    : tempo_utils::TypedStatus<SandboxCondition>(statusCode, detail)
{
}

bool
chord_sandbox::SandboxStatus::convert(SandboxStatus &dstStatus, const tempo_utils::Status &srcStatus)
{
    std::string_view srcNs = srcStatus.getErrorCategory();
    std::string_view dstNs = kChordSandboxStatusNs;
    if (srcNs != dstNs)
        return false;
    dstStatus = SandboxStatus(srcStatus.getStatusCode(), srcStatus.getDetail());
    return true;
}