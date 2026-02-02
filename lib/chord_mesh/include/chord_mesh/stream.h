#ifndef CHORD_MESH_STREAM_H
#define CHORD_MESH_STREAM_H

#include <tempo_utils/uuid.h>

#include "envelope.h"
#include "stream_io.h"
#include "stream_manager.h"

namespace chord_mesh {

    class Stream {
    public:
        explicit Stream(StreamHandle *handle);
        virtual ~Stream();

        bool isInitiator() const;
        bool isSecure() const;

        tempo_utils::UUID getId() const;
        StreamState getStreamState() const;

        tempo_utils::Status start(std::unique_ptr<AbstractStreamContext> &&ctx);
        tempo_utils::Status negotiate(std::string_view protocolName);
        tempo_utils::Status send(
            EnvelopeVersion version,
            std::shared_ptr<const tempo_utils::ImmutableBytes> payload,
            absl::Time timestamp = {});
        void shutdown();

    private:
        StreamHandle *m_handle;

        friend class StreamAcceptor;
        friend class StreamConnector;
    };
}

#endif // CHORD_MESH_STREAM_H