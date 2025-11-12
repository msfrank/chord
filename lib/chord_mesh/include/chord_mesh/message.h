#ifndef CHORD_MESH_MESSAGE_H
#define CHORD_MESH_MESSAGE_H

#include <tempo_security/certificate_key_pair.h>
#include <tempo_security/digest_utils.h>
#include <tempo_utils/bytes_appender.h>
#include <tempo_utils/immutable_bytes.h>
#include <tempo_utils/result.h>

namespace chord_mesh {

    constexpr tu_uint32 kMessageVersionStream = 0xFF;
    constexpr tu_uint32 kMessageVersion1 = 1;
    constexpr tu_uint32 kMessageSignedFlag = 1;
    constexpr tu_uint32 kMaxPayloadSize = 16777216;     // 2^24

    enum class MessageVersion {
        Invalid,
        Stream,
        Version1,
    };

    class Message {
    public:
        Message();
        explicit Message(
            tu_uint8 version,
            tu_uint8 flags,
            absl::Time timestamp,
            std::shared_ptr<const tempo_utils::ImmutableBytes> payload = {},
            const tempo_security::Digest &digest = {});
        Message(const Message &other);

        bool isValid() const;

        MessageVersion getVersion() const;
        bool isSigned() const;

        absl::Time getTimestamp() const;
        void setTimestamp(absl::Time timestamp);

        bool hasPayload() const;
        std::shared_ptr<const tempo_utils::ImmutableBytes> getPayload() const;
        void setPayload(std::shared_ptr<const tempo_utils::ImmutableBytes> payload);

        bool hasDigest() const;
        tempo_security::Digest getDigest() const;
        void setDigest(const tempo_security::Digest &digest);

    private:
        struct Priv {
            tu_uint8 version;
            tu_uint8 flags;
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

        MessageVersion getVersion() const;
        void setVersion(MessageVersion version);

        absl::Time getTimestamp() const;
        void setTimestamp(absl::Time timestamp);

        std::shared_ptr<const tempo_utils::ImmutableBytes> getPayload() const;
        tempo_utils::Status setPayload(std::shared_ptr<const tempo_utils::ImmutableBytes> payload);

        std::shared_ptr<tempo_security::PrivateKey> getPrivateKey() const;
        void setPrivateKey(std::shared_ptr<tempo_security::PrivateKey> privateKey);

        tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>> toBytes() const;

        void reset();

    private:
        MessageVersion m_version;
        absl::Time m_timestamp;
        std::shared_ptr<const tempo_utils::ImmutableBytes> m_payload;
        std::shared_ptr<tempo_security::PrivateKey> m_privateKey;
    };

    class MessageParser {
    public:
        MessageParser();

        std::shared_ptr<tempo_security::X509Certificate> getCertificate() const;
        void setCertificate(std::shared_ptr<tempo_security::X509Certificate> certificate);

        tempo_utils::Status pushBytes(std::span<const tu_uint8> bytes);

        tempo_utils::Status checkReady(bool &ready);
        tempo_utils::Status takeReady(Message &message);

        bool hasPending() const;
        std::shared_ptr<const tempo_utils::MemoryBytes> popPending();

        void reset();

    private:
        std::shared_ptr<tempo_security::X509Certificate> m_certificate;
        std::unique_ptr<tempo_utils::BytesAppender> m_pending;
        bool m_ready;
        tu_uint8 m_messageVersion;
        tu_uint8 m_messageFlags;
        tu_uint32 m_timestamp;
        tu_uint32 m_payloadSize;
        tu_uint8 m_digestSize;
    };
}

#endif // CHORD_MESH_MESSAGE_H