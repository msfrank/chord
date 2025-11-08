
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/message.h>
#include <tempo_utils/big_endian.h>
#include <tempo_utils/bytes_appender.h>

chord_mesh::Message::Message()
{
}

chord_mesh::Message::Message(
    absl::Time timestamp,
    std::shared_ptr<const tempo_utils::ImmutableBytes> payload,
    const tempo_security::Digest &digest)
    : m_priv(std::make_shared<Priv>())
{
    m_priv->timestamp = timestamp;
    m_priv->payload = std::move(payload);
    m_priv->digest = digest;
}

chord_mesh::Message::Message(const Message &other)
    : m_priv(other.m_priv)
{
}

bool
chord_mesh::Message::isValid() const
{
    return m_priv != nullptr;
}

absl::Time
chord_mesh::Message::getTimestamp() const
{
    if (m_priv == nullptr)
        return {};
    return m_priv->timestamp;
}

void
chord_mesh::Message::setTimestamp(absl::Time timestamp)
{
    if (m_priv == nullptr) {
        m_priv = std::make_shared<Priv>();
    }
    m_priv->timestamp = timestamp;
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::Message::getPayload() const
{
    if (m_priv == nullptr)
        return {};
    return m_priv->payload;
}

void
chord_mesh::Message::setPayload(std::shared_ptr<const tempo_utils::ImmutableBytes> payload)
{
    if (m_priv == nullptr) {
        m_priv = std::make_shared<Priv>();
    }
    m_priv->payload = std::move(payload);
}

tempo_security::Digest
chord_mesh::Message::getDigest() const
{
    if (m_priv == nullptr)
        return {};
    return m_priv->digest;
}

void
chord_mesh::Message::setDigest(const tempo_security::Digest &digest)
{
    if (m_priv == nullptr) {
        m_priv = std::make_shared<Priv>();
    }
    m_priv->digest = digest;
}

chord_mesh::MessageBuilder::MessageBuilder(std::shared_ptr<const tempo_utils::ImmutableBytes> payload)
    : m_payload(std::move(payload))
{
}

absl::Time
chord_mesh::MessageBuilder::getTimestamp() const
{
    return m_timestamp;
}

void
chord_mesh::MessageBuilder::setTimestamp(absl::Time timestamp)
{
    m_timestamp = timestamp;
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::MessageBuilder::getPayload() const
{
    return m_payload;
}

tempo_utils::Status
chord_mesh::MessageBuilder::setPayload(std::shared_ptr<const tempo_utils::ImmutableBytes> payload)
{
    m_payload = payload;
    return {};
}

std::shared_ptr<tempo_security::PrivateKey>
chord_mesh::MessageBuilder::getPrivateKey() const
{
    return m_privateKey;
}

void
chord_mesh::MessageBuilder::setPrivateKey(std::shared_ptr<tempo_security::PrivateKey> privateKey)
{
    m_privateKey = privateKey;
}

tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>>
chord_mesh::MessageBuilder::toBytes() const
{
    if (m_payload == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "missing message payload");
    tu_uint32 payloadSize = m_payload->getSize();
    if (payloadSize > kMaxPayloadSize)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "message payload is too large");

    tempo_utils::BytesAppender appender;

    // append the version field
    appender.appendU8(kMessageVersion1);

    // append the flags field
    tu_uint8 flags = 0;
    if (m_privateKey != nullptr) {
        flags |= kMessageSignedFlag;
    }
    appender.appendU8(flags);

    // append the timestamp field
    tu_uint32 timestamp = absl::ToUnixSeconds(m_timestamp);
    if (timestamp == 0) {
        timestamp = absl::ToUnixSeconds(absl::Now());
    }
    appender.appendU32(timestamp);

    // append the payload size field
    appender.appendU32(payloadSize);

    // append the payload
    appender.appendBytes(m_payload->getSpan());

    // if message is signed then generate the signature
    if (flags & kMessageSignedFlag) {
        std::span data(appender.getData(), appender.getSize());

        tempo_security::Digest digest;
        TU_ASSIGN_OR_RETURN (digest, tempo_security::DigestUtils::generate_signed_message_digest(
            data, m_privateKey, tempo_security::DigestId::None));

        // append the digest size field
        if (std::numeric_limits<tu_uint8>::max() < digest.getSize())
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "message digest is too large");
        appender.appendU8(static_cast<tu_uint8>(digest.getSize()));

        // append the digest
        appender.appendBytes(digest.getSpan());
    }

    // return the serialized bytes
    auto bytes = appender.finish();
    return std::static_pointer_cast<const tempo_utils::ImmutableBytes>(bytes);
}

void
chord_mesh::MessageBuilder::reset()
{
    m_payload = {};
}

chord_mesh::MessageParser::MessageParser()
{
    reset();
}

std::shared_ptr<tempo_security::X509Certificate>
chord_mesh::MessageParser::getCertificate() const
{
    return m_certificate;
}

void
chord_mesh::MessageParser::setCertificate(std::shared_ptr<tempo_security::X509Certificate> certificate)
{
    m_certificate = certificate;
}

tempo_utils::Status
chord_mesh::MessageParser::pushBytes(std::span<const tu_uint8> bytes)
{
    m_pending->appendBytes(bytes);

    // get the message version if we have read enough input
    if (m_messageVersion == 0) {
        if (m_pending->getSize() >= 1) {
            auto *ptr = m_pending->getData();
            m_messageVersion = tempo_utils::read_u8_and_advance(ptr);
        }
    }

    //
    if (m_messageVersion != 1)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid message version");

    // get the message version and payload size if we have read enough input
    if (m_payloadSize == 0) {
        if (m_pending->getSize() >= 10) {
            auto *ptr = m_pending->getData();
            m_messageVersion = tempo_utils::read_u8_and_advance(ptr);
            m_messageFlags = tempo_utils::read_u8_and_advance(ptr);
            m_timestamp = tempo_utils::read_u32_and_advance(ptr);
            m_payloadSize = tempo_utils::read_u32_and_advance(ptr);
        }
    }

    // we haven't read enough input to get the payload size
    if (m_payloadSize == 0)
        return {};

    bool verificationRequired = m_messageFlags & kMessageSignedFlag;

    // fail if message is signed and no certificate is present
    if (verificationRequired && m_certificate == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "cannot verify signed message");

    // we haven't read enough input to parse the payload
    if (m_pending->getSize() < 10 + m_payloadSize)
        return {};

    tu_uint32 trailerSize;

    if (verificationRequired) {

        // get the digest size if we have read enough input
        if (m_digestSize == 0) {
            if (m_pending->getSize() >= 10 + m_payloadSize + 1) {
                auto *ptr = m_pending->getData() + 10 + m_payloadSize;
                m_digestSize = tempo_utils::read_u8_and_advance(ptr);
            }
        }

        // we haven't read enough input to get the digest size
        if (m_digestSize == 0)
            return {};

        trailerSize = 1 + m_digestSize;

        // we haven't read enough input to parse the digest
        if (m_pending->getSize() < 10 + m_payloadSize + 1 + m_digestSize)
            return {};
    } else {
        trailerSize = 0;
    }

    // finish the appender
    auto pending = m_pending->finish()->toSlice();
    auto messageSize = 10 + m_payloadSize + trailerSize;
    auto timestamp = absl::FromUnixSeconds(m_timestamp);
    auto payloadSize = m_payloadSize;
    auto digestSize = m_digestSize;

    // reset parser state
    reset();

    // if there is additional data then add it to the appender
    auto remainder = pending.slice(messageSize, pending.getSize() - messageSize);
    if (!remainder.isEmpty()) {
        m_pending->appendBytes(remainder.sliceView());
    }

    // construct the message
    Message message(timestamp);
    auto payloadBytes = pending.slice(10, payloadSize).sliceView();

    // verify the signature against the public key
    if (verificationRequired) {
        auto verifyBytes = pending.slice(0, 10 + payloadSize).sliceView();
        auto digestBytes = pending.slice(10 + payloadSize + 1, digestSize).sliceView();
        tempo_security::Digest digest(digestBytes);
        bool verified;
        TU_ASSIGN_OR_RETURN (verified, tempo_security::DigestUtils::verify_signed_message_digest(
            verifyBytes, digest, m_certificate));
        if (!verified)
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "signature verification failed");
        message.setDigest(digest);
    }

    message.setPayload(tempo_utils::MemoryBytes::copy(payloadBytes));

    m_messages.push(message);
    return {};
}

bool
chord_mesh::MessageParser::hasMessage() const
{
    return !m_messages.empty();
}

size_t
chord_mesh::MessageParser::numMessages() const
{
    return m_messages.size();
}

chord_mesh::Message
chord_mesh::MessageParser::popMessage()
{
    if (m_messages.empty())
        return {};
    auto message = m_messages.front();
    m_messages.pop();
    return message;
}

void
chord_mesh::MessageParser::reset()
{
    m_pending = std::make_unique<tempo_utils::BytesAppender>();
    m_messageVersion = 0;
    m_timestamp = 0;
    m_payloadSize = 0;
    m_digestSize = 0;
}