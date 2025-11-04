#ifndef CHORD_MESH_MESH_RESULT_H
#define CHORD_MESH_MESH_RESULT_H

#include <string>

#include <fmt/core.h>
#include <fmt/format.h>

#include <tempo_utils/log_stream.h>
#include <tempo_utils/status.h>

namespace chord_mesh {

    constexpr const char *kChordMeshStatusNs("dev.zuri.ns:chord-mesh-status-1");

    enum class MeshCondition {
        kMeshInvariant,
    };


    class MeshStatus : public tempo_utils::TypedStatus<MeshCondition> {
    public:
        using TypedStatus::TypedStatus;
        static bool convert(MeshStatus &dstStatus, const tempo_utils::Status &srcStatus);

    private:
        MeshStatus(tempo_utils::StatusCode statusCode, std::shared_ptr<const tempo_utils::Detail> detail);

    public:
        /**
         *
         * @param condition
         * @param message
         * @return
         */
        static MeshStatus forCondition(
            MeshCondition condition,
            std::string_view message)
        {
            return MeshStatus(condition, message);
        }
        /**
         *
         * @tparam Args
         * @param condition
         * @param messageFmt
         * @param messageArgs
         * @return
         */
        template <typename... Args>
        static MeshStatus forCondition(
            MeshCondition condition,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return MeshStatus(condition, message);
        }
        /**
         *
         * @tparam Args
         * @param condition
         * @param messageFmt
         * @param messageArgs
         * @return
         */
        template <typename... Args>
        static MeshStatus forCondition(
            MeshCondition condition,
            tempo_utils::TraceId traceId,
            tempo_utils::SpanId spanId,
            fmt::string_view messageFmt = {},
            Args... messageArgs)
        {
            auto message = fmt::vformat(messageFmt, fmt::make_format_args(messageArgs...));
            return MeshStatus(condition, message, traceId, spanId);
        }
    };
}

namespace tempo_utils {

    template<>
    struct StatusTraits<chord_mesh::MeshCondition> {
        using ConditionType = chord_mesh::MeshCondition;
        static bool convert(chord_mesh::MeshStatus &dstStatus, const tempo_utils::Status &srcStatus)
        {
            return chord_mesh::MeshStatus::convert(dstStatus, srcStatus);
        }
    };

    template<>
    struct ConditionTraits<chord_mesh::MeshCondition> {
        using StatusType = chord_mesh::MeshStatus;
        static constexpr const char *condition_namespace() { return chord_mesh::kChordMeshStatusNs; }
        static constexpr StatusCode make_status_code(chord_mesh::MeshCondition condition)
        {
            switch (condition) {
                case chord_mesh::MeshCondition::kMeshInvariant:
                    return tempo_utils::StatusCode::kInternal;
                default:
                    return tempo_utils::StatusCode::kUnknown;
            }
        };
        static constexpr const char *make_error_message(chord_mesh::MeshCondition condition)
        {
            switch (condition) {
                case chord_mesh::MeshCondition::kMeshInvariant:
                    return "Mesh invariant";
                default:
                    return "INVALID";
            }
        }
    };
}

#endif // CHORD_MESH_MESH_RESULT_H