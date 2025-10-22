
#include <chord_common/grpc_status.h>

grpc::Status
chord_common::convert_status(const tempo_utils::Status &status)
{
    grpc::StatusCode code;

    switch (status.getStatusCode()) {
        case tempo_utils::StatusCode::kCancelled:
            code = grpc::StatusCode::CANCELLED;
            break;
        case tempo_utils::StatusCode::kInvalidArgument:
            code = grpc::StatusCode::INVALID_ARGUMENT;
            break;
        case tempo_utils::StatusCode::kDeadlineExceeded:
            code = grpc::StatusCode::DEADLINE_EXCEEDED;
            break;
        case tempo_utils::StatusCode::kNotFound:
            code = grpc::StatusCode::NOT_FOUND;
            break;
        case tempo_utils::StatusCode::kAlreadyExists:
            code = grpc::StatusCode::ALREADY_EXISTS;
            break;
        case tempo_utils::StatusCode::kPermissionDenied:
            code = grpc::StatusCode::PERMISSION_DENIED;
            break;
        case tempo_utils::StatusCode::kUnauthenticated:
            code = grpc::StatusCode::UNAUTHENTICATED;
            break;
        case tempo_utils::StatusCode::kResourceExhausted:
            code = grpc::StatusCode::RESOURCE_EXHAUSTED;
            break;
        case tempo_utils::StatusCode::kFailedPrecondition:
            code = grpc::StatusCode::FAILED_PRECONDITION;
            break;
        case tempo_utils::StatusCode::kAborted:
            code = grpc::StatusCode::ABORTED;
            break;
        case tempo_utils::StatusCode::kUnavailable:
            code = grpc::StatusCode::UNAVAILABLE;
            break;
        case tempo_utils::StatusCode::kOutOfRange:
            code = grpc::StatusCode::OUT_OF_RANGE;
            break;
        case tempo_utils::StatusCode::kUnimplemented:
            code = grpc::StatusCode::UNIMPLEMENTED;
            break;
        case tempo_utils::StatusCode::kInternal:
            code = grpc::StatusCode::INTERNAL;
            break;
        case tempo_utils::StatusCode::kUnknown:
        default:
            code = grpc::StatusCode::UNKNOWN;
            break;
    }

    auto message = status.getMessage();
    return grpc::Status(code, std::string(message));
}
