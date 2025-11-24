#ifndef CHORD_MESH_ABSTRACT_MESH_NETWORK_H
#define CHORD_MESH_ABSTRACT_MESH_NETWORK_H
#include <tempo_utils/status.h>

namespace chord_mesh {

    template<class MessageType>
    class AbstractMeshNetwork {
    public:
        virtual ~AbstractMeshNetwork() = default;

        virtual tempo_utils::Status send(const MessageType &message) = 0;

        virtual tempo_utils::Status broadcast(const MessageType &message) = 0;

    };
}
#endif // CHORD_MESH_ABSTRACT_MESH_NETWORK_H