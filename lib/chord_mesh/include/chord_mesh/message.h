#ifndef CHORD_MESH_MESSAGE_H
#define CHORD_MESH_MESSAGE_H

#include <tempo_security/certificate_key_pair.h>
#include <tempo_security/digest_utils.h>
#include <tempo_utils/bytes_appender.h>
#include <tempo_utils/immutable_bytes.h>
#include <tempo_utils/result.h>

namespace chord_mesh {

    class Message {
    public:
        Message();
        Message(
            absl::Time timestamp,
            std::shared_ptr<const tempo_utils::ImmutableBytes> payload = {},
            const tempo_security::Digest &digest = {});
        Message(const Message &other);

        bool isValid() const;

        absl::Time getTimestamp() const;
        void setTimestamp(absl::Time timestamp);

        std::shared_ptr<const tempo_utils::ImmutableBytes> getPayload() const;
        void setPayload(std::shared_ptr<const tempo_utils::ImmutableBytes> payload);

        tempo_security::Digest getDigest() const;
        void setDigest(const tempo_security::Digest &digest);

    private:
        struct Priv {
            absl::Time timestamp;
            std::shared_ptr<const tempo_utils::ImmutableBytes> payload;
            tempo_security::Digest digest;
        };
        std::shared_ptr<Priv> m_priv;
    };

    /**
     *
     */
    class MessageBuilder {
    public:
        explicit MessageBuilder(std::shared_ptr<const tempo_utils::ImmutableBytes> payload = {});

        absl::Time getTimestamp() const;
        void setTimestamp(absl::Time timestamp);

        std::shared_ptr<const tempo_utils::ImmutableBytes> getPayload() const;
        tempo_utils::Status setPayload(std::shared_ptr<const tempo_utils::ImmutableBytes> payload);

        std::filesystem::path getPemPrivateKeyFile() const;
        void setPemPrivateKeyFile(const std::filesystem::path &pemPrivateKeyFile);

        tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>> toBytes() const;

        void reset();

    private:
        absl::Time m_timestamp;
        std::shared_ptr<const tempo_utils::ImmutableBytes> m_payload;
        std::filesystem::path m_pemPrivateKeyFile;
    };

    class MessageParser {
    public:
        MessageParser();

        std::filesystem::path getPemCertificateFile() const;
        void setPemCertificateFile(const std::filesystem::path &pemCertificateFile);

        tempo_utils::Status pushBytes(std::span<const tu_uint8> bytes);

        bool hasMessage() const;
        size_t numMessages() const;
        Message popMessage();

        void reset();

    private:
        std::filesystem::path m_pemCertificateFile;
        std::queue<Message> m_messages;
        std::unique_ptr<tempo_utils::BytesAppender> m_pending;
        tu_uint8 m_messageVersion;
        tu_uint32 m_timestamp;
        tu_uint32 m_payloadSize;
        tu_uint8 m_digestSize;
    };
}

#endif // CHORD_MESH_MESSAGE_H