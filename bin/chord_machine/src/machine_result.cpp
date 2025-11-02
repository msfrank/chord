
#include <chord_machine/machine_result.h>

chord_machine::MachineStatus::MachineStatus(
    tempo_utils::StatusCode statusCode,
    std::shared_ptr<const tempo_utils::Detail> detail)
    : tempo_utils::TypedStatus<MachineCondition>(statusCode, detail)
{
}

bool
chord_machine::MachineStatus::convert(MachineStatus &dstStatus, const tempo_utils::Status &srcStatus)
{
    std::string_view srcNs = srcStatus.getErrorCategory();
    std::string_view dstNs = kChordMachineStatusNs;
    if (srcNs != dstNs)
        return false;
    dstStatus = MachineStatus(srcStatus.getStatusCode(), srcStatus.getDetail());
    return true;
}
