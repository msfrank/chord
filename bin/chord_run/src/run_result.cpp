
#include <absl/strings/substitute.h>

#include <chord_run/run_result.h>

chord_run::RunStatus::RunStatus(
    tempo_utils::StatusCode statusCode,
    std::shared_ptr<const tempo_utils::Detail> detail)
    : tempo_utils::TypedStatus<RunCondition>(statusCode, detail)
{
}

bool
chord_run::RunStatus::convert(RunStatus &dstStatus, const tempo_utils::Status &srcStatus)
{
    std::string_view srcNs = srcStatus.getErrorCategory();
    std::string_view dstNs = kChordRunStatusNs;
    if (srcNs != dstNs)
        return false;
    dstStatus = RunStatus(srcStatus.getStatusCode(), srcStatus.getDetail());
    return true;
}
