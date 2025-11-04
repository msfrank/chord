
#include <chord_mesh/mesh_result.h>

chord_mesh::MeshStatus::MeshStatus(
    tempo_utils::StatusCode statusCode,
    std::shared_ptr<const tempo_utils::Detail> detail)
    : tempo_utils::TypedStatus<MeshCondition>(statusCode, detail)
{
}

bool
chord_mesh::MeshStatus::convert(MeshStatus &dstStatus, const tempo_utils::Status &srcStatus)
{
    std::string_view srcNs = srcStatus.getErrorCategory();
    std::string_view dstNs = kChordMeshStatusNs;
    if (srcNs != dstNs)
        return false;
    dstStatus = MeshStatus(srcStatus.getStatusCode(), srcStatus.getDetail());
    return true;
}
