#ifndef CHORD_MESH_CONNECT_H
#define CHORD_MESH_CONNECT_H

#include <tempo_utils/uuid.h>

#include "stream_manager.h"

namespace chord_mesh {

    class Connect {
    public:
        explicit Connect(ConnectHandle *handle);
        virtual ~Connect();

        tempo_utils::UUID getId() const;
        ConnectState getConnectState() const;

        void abort();

    private:
        ConnectHandle *m_handle;
        tempo_utils::UUID m_id;
        ConnectState m_state;

        friend class StreamConnector;
    };

}

#endif // CHORD_MESH_CONNECT_H