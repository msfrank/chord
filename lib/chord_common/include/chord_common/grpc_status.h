#ifndef CHORD_COMMON_GRPC_STATUS_H
#define CHORD_COMMON_GRPC_STATUS_H

#include <grpcpp/impl/status.h>

#include <tempo_utils/result.h>
#include <tempo_utils/status.h>

namespace chord_common {

    grpc::Status convert_status(const tempo_utils::Status &status);

    template<typename ResultType>
    grpc::Status convert_result(const tempo_utils::Result<ResultType> &result)
    {
        if (result.isResult())
            return grpc::Status{};
        return convert_status(result.getStatus());
    }
}

#endif // CHORD_COMMON_GRPC_STATUS_H