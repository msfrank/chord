
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/envelope.h>
#include <tempo_utils/big_endian.h>
#include <tempo_utils/bytes_appender.h>

chord_mesh::Envelope::Envelope()
{
}

chord_mesh::Envelope::Envelope(
    tu_uint8 version,
    tu_uint8 flags,
    absl::Time timestamp,
    std::shared_ptr<const tempo_utils::ImmutableBytes> payload,
    const tempo_security::Digest &digest)
    : m_priv(std::make_shared<Priv>())
{
    m_priv->version = version;
    m_priv->flags = flags;
    m_priv->timestamp = timestamp;
    m_priv->payload = std::move(payload);
    m_priv->digest = digest;
}

chord_mesh::Envelope::Envelope(const Envelope &other)
    : m_priv(other.m_priv)
{
}

bool
chord_mesh::Envelope::isValid() const
{
    return m_priv != nullptr;
}

chord_mesh::EnvelopeVersion
chord_mesh::Envelope::getVersion() const
{
    if (m_priv == nullptr)
        return EnvelopeVersion::Invalid;
    switch (m_priv->version) {
        case kEnvelopeVersionStream:
            return EnvelopeVersion::Stream;
        case kEnvelopeVersion1:
            return EnvelopeVersion::Version1;
        default:
            return EnvelopeVersion::Invalid;
    }
}

bool
chord_mesh::Envelope::isSigned() const
{
    if (m_priv == nullptr)
        return false;
    return m_priv->flags & kEnvelopeSignedFlag;
}

absl::Time
chord_mesh::Envelope::getTimestamp() const
{
    if (m_priv == nullptr)
        return {};
    return m_priv->timestamp;
}

void
chord_mesh::Envelope::setTimestamp(absl::Time timestamp)
{
    if (m_priv == nullptr) {
        m_priv = std::make_shared<Priv>();
    }
    m_priv->timestamp = timestamp;
}

bool
chord_mesh::Envelope::hasHeader() const
{
    if (m_priv == nullptr)
        return false;
    return m_priv->header != nullptr;
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::Envelope::getHeader() const
{
    if (m_priv == nullptr)
        return {};
    return m_priv->header;
}

void
chord_mesh::Envelope::setHeader(std::shared_ptr<const tempo_utils::ImmutableBytes> header)
{
    if (m_priv == nullptr) {
        m_priv = std::make_shared<Priv>();
    }
    m_priv->header = std::move(header);
}

bool
chord_mesh::Envelope::hasPayload() const
{
    if (m_priv == nullptr)
        return false;
    return m_priv->payload != nullptr;
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::Envelope::getPayload() const
{
    if (m_priv == nullptr)
        return {};
    return m_priv->payload;
}

void
chord_mesh::Envelope::setPayload(std::shared_ptr<const tempo_utils::ImmutableBytes> payload)
{
    if (m_priv == nullptr) {
        m_priv = std::make_shared<Priv>();
    }
    m_priv->payload = std::move(payload);
}

tempo_security::Digest
chord_mesh::Envelope::getDigest() const
{
    if (m_priv == nullptr)
        return {};
    return m_priv->digest;
}

void
chord_mesh::Envelope::setDigest(const tempo_security::Digest &digest)
{
    if (m_priv == nullptr) {
        m_priv = std::make_shared<Priv>();
    }
    m_priv->digest = digest;
}

chord_mesh::EnvelopeBuilder::EnvelopeBuilder(std::shared_ptr<const tempo_utils::ImmutableBytes> payload)
    : m_version(EnvelopeVersion::Invalid),
      m_payload(std::move(payload))
{
}

chord_mesh::EnvelopeVersion
chord_mesh::EnvelopeBuilder::getVersion() const
{
    return m_version;
}

void
chord_mesh::EnvelopeBuilder::setVersion(EnvelopeVersion version)
{
    m_version = version;
}

absl::Time
chord_mesh::EnvelopeBuilder::getTimestamp() const
{
    return m_timestamp;
}

void
chord_mesh::EnvelopeBuilder::setTimestamp(absl::Time timestamp)
{
    m_timestamp = timestamp;
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::EnvelopeBuilder::getHeader() const
{
    return m_header;
}

tempo_utils::Status
chord_mesh::EnvelopeBuilder::setHeader(std::shared_ptr<const tempo_utils::ImmutableBytes> header)
{
    m_header = std::move(header);
    return {};
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::EnvelopeBuilder::getPayload() const
{
    return m_payload;
}

tempo_utils::Status
chord_mesh::EnvelopeBuilder::setPayload(std::shared_ptr<const tempo_utils::ImmutableBytes> payload)
{
    m_payload = std::move(payload);
    return {};
}

std::shared_ptr<tempo_security::PrivateKey>
chord_mesh::EnvelopeBuilder::getPrivateKey() const
{
    return m_privateKey;
}

void
chord_mesh::EnvelopeBuilder::setPrivateKey(std::shared_ptr<tempo_security::PrivateKey> privateKey)
{
    m_privateKey = std::move(privateKey);
}

tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>>
chord_mesh::EnvelopeBuilder::toBytes() const
{
    if (m_payload == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "missing envelope payload");
    tu_uint32 payloadSize = m_payload->getSize();
    if (payloadSize > kMaxPayloadSize)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "envelope payload is too large");

    tu_uint32 headerSizeU32 = m_header? m_header->getSize() : 0;
    if (headerSizeU32 > kMaxHeaderSize)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "envelope payload is too large");
    auto headerSize = static_cast<tu_uint16>(headerSizeU32);

    tempo_utils::BytesAppender appender;

    // append the version field
    tu_uint8 version;
    switch (m_version) {
        case EnvelopeVersion::Stream:
            version = kEnvelopeVersionStream;
            break;
        case EnvelopeVersion::Version1:
            version = kEnvelopeVersion1;
            break;
        default:
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "invalid envelope version");
    }
    appender.appendU8(version);

    // append the flags field
    tu_uint8 flags = 0;
    if (m_privateKey != nullptr) {
        flags |= kEnvelopeSignedFlag;
    }
    appender.appendU8(flags);

    // append the timestamp field
    tu_uint32 timestamp = absl::ToUnixSeconds(m_timestamp);
    if (timestamp == 0) {
        timestamp = absl::ToUnixSeconds(absl::Now());
    }
    appender.appendU32(timestamp);

    // append the header size field
    appender.appendU16(headerSize);

    // append the payload size field
    appender.appendU32(payloadSize);

    // append the header if present
    if (headerSize > 0) {
        appender.appendBytes(m_header->getSpan());
    }

    // append the payload
    appender.appendBytes(m_payload->getSpan());

    // if envelope is signed then generate the signature
    if (flags & kEnvelopeSignedFlag) {
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
chord_mesh::EnvelopeBuilder::reset()
{
    m_payload = {};
}

chord_mesh::EnvelopeParser::EnvelopeParser()
{
    reset();
}

std::shared_ptr<tempo_security::X509Certificate>
chord_mesh::EnvelopeParser::getCertificate() const
{
    return m_certificate;
}

void
chord_mesh::EnvelopeParser::setCertificate(std::shared_ptr<tempo_security::X509Certificate> certificate)
{
    m_certificate = certificate;
}

tempo_utils::Status
chord_mesh::EnvelopeParser::pushBytes(std::span<const tu_uint8> bytes)
{
    m_pending->appendBytes(bytes);
    return {};
}

tempo_utils::Status
chord_mesh::EnvelopeParser::checkReady(bool &ready)
{
    if (m_ready) {
        ready = true;
        return {};
    }

    ready = false;

    // get the envelope version if we have read enough input
    if (m_envelopeVersion == 0) {
        if (m_pending->getSize() < 2)
            return {};
        auto *ptr = m_pending->getData();
        m_envelopeVersion = tempo_utils::read_u8_and_advance(ptr);
    }

    // validate envelope version
    switch (m_envelopeVersion) {
        case kEnvelopeVersionStream:
        case kEnvelopeVersion1:
            break;
        default:
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "invalid envelope version");
    }

    // get the envelope version and payload size if we have read enough input
    if (m_payloadSize == 0) {
        if (m_pending->getSize() < 12)
            return {};
        auto *ptr = m_pending->getData() + 1;
        m_envelopeFlags = tempo_utils::read_u8_and_advance(ptr);
        m_timestamp = tempo_utils::read_u32_and_advance(ptr);
        m_headerSize = tempo_utils::read_u16_and_advance(ptr);
        m_payloadSize = tempo_utils::read_u32_and_advance(ptr);
        if (m_payloadSize == 0)
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "payload size must be greater than 0");
    }

    bool verificationRequired = m_envelopeFlags & kEnvelopeSignedFlag;

    // fail if envelope is signed and no certificate is present
    if (verificationRequired && m_certificate == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "cannot verify signed envelope");

    // we haven't read enough input to parse the header and payload
    if (m_pending->getSize() < 12 + m_headerSize + m_payloadSize)
        return {};

    if (verificationRequired) {

        // get the digest size if we have read enough input
        if (m_digestSize == 0) {
            if (m_pending->getSize() < 12 + m_headerSize + m_payloadSize + 1)
                return {};
            auto *ptr = m_pending->getData() + 12 + m_headerSize + m_payloadSize;
            m_digestSize = tempo_utils::read_u8_and_advance(ptr);
            if (m_digestSize == 0)
                return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                    "digest size must be greater than 0");
        }

        // we haven't read enough input to parse the digest
        if (m_pending->getSize() < 12 + m_headerSize + m_payloadSize + 1 + m_digestSize)
            return {};
    }

    m_ready = true;
    ready = true;
    return {};
}

tempo_utils::Status
chord_mesh::EnvelopeParser::takeReady(Envelope &ready)
{
    if (!m_ready)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "no ready envelope available");

    // finish the appender
    auto pending = m_pending->finish()->toSlice();

    // copy parser state before reset
    auto trailerSize = m_digestSize > 0? m_digestSize + 1 : 0;
    auto envelopeSize = 12 + m_headerSize + m_payloadSize + trailerSize;
    auto headerSize = m_headerSize;
    auto payloadSize = m_payloadSize;
    auto digestSize = m_digestSize;
    auto envelopeVersion = m_envelopeVersion;
    auto envelopeFlags = m_envelopeFlags;
    auto timestamp = absl::FromUnixSeconds(m_timestamp);
    bool verificationRequired = envelopeFlags & kEnvelopeSignedFlag;

    // reset parser state
    reset();

    // if there is additional data then add it to the appender
    auto remainder = pending.slice(envelopeSize, pending.getSize() - envelopeSize);
    if (!remainder.isEmpty()) {
        m_pending->appendBytes(remainder.sliceView());
    }

    // construct the envelope
    Envelope envelope(envelopeVersion, envelopeFlags, timestamp);
    auto headerBytes = pending.slice(12, headerSize).sliceView();
    auto payloadBytes = pending.slice(12 + headerSize, payloadSize).sliceView();

    // verify the signature against the public key
    if (verificationRequired) {
        auto verifyBytes = pending
            .slice(0, 12 + headerSize + payloadSize)
            .sliceView();
        auto digestBytes = pending
            .slice(12 + headerSize + payloadSize + 1, digestSize)
            .sliceView();
        tempo_security::Digest digest(digestBytes);
        bool verified;
        TU_ASSIGN_OR_RETURN (verified, tempo_security::DigestUtils::verify_signed_message_digest(
            verifyBytes, digest, m_certificate));
        if (!verified)
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "signature verification failed");
        envelope.setDigest(digest);
    }

    envelope.setPayload(tempo_utils::MemoryBytes::copy(payloadBytes));
    if (!headerBytes.empty()) {
        envelope.setHeader(tempo_utils::MemoryBytes::copy(headerBytes));
    }

    ready = envelope;

    return {};
}

bool
chord_mesh::EnvelopeParser::hasPending() const
{
    return m_pending->getSize() > 0;
}

std::shared_ptr<const tempo_utils::MemoryBytes>
chord_mesh::EnvelopeParser::popPending()
{
    auto pending = m_pending->finish();
    reset();
    return pending;
}

void
chord_mesh::EnvelopeParser::reset()
{
    m_pending = std::make_unique<tempo_utils::BytesAppender>();
    m_ready = false;
    m_envelopeVersion = 0;
    m_envelopeFlags = 0;
    m_timestamp = 0;
    m_headerSize = 0;
    m_payloadSize = 0;
    m_digestSize = 0;
}