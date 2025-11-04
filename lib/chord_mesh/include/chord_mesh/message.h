#ifndef CHORD_MESH_MESSAGE_H
#define CHORD_MESH_MESSAGE_H

#include <tempo_utils/bytes_appender.h>
#include <tempo_utils/immutable_bytes.h>
#include <tempo_utils/result.h>

namespace chord_mesh {

    class MessageBuilder {
    public:
        explicit MessageBuilder(std::shared_ptr<const tempo_utils::ImmutableBytes> payload = {});

        std::shared_ptr<const tempo_utils::ImmutableBytes> getPayload() const;
        tempo_utils::Status setPayload(std::shared_ptr<const tempo_utils::ImmutableBytes> payload);

        tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>> toBytes() const;

        void reset();

    private:
        std::shared_ptr<const tempo_utils::ImmutableBytes> m_payload;
    };

    class MessageParser {
    public:
        MessageParser();

        bool appendBytes(std::span<const tu_uint8> bytes);

        bool hasMessage() const;
        std::shared_ptr<const tempo_utils::ImmutableBytes> takeMessage();

    private:
        std::queue<std::shared_ptr<const tempo_utils::ImmutableBytes>> m_messages;
        std::unique_ptr<tempo_utils::BytesAppender> m_pending;
        tu_uint8 m_messageVersion;
        tu_uint32 m_messageSize;
    };
}

#endif // CHORD_MESH_MESSAGE_H