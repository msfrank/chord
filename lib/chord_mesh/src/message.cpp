
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/message.h>
#include <tempo_utils/big_endian.h>
#include <tempo_utils/bytes_appender.h>

chord_mesh::MessageBuilder::MessageBuilder(std::shared_ptr<const tempo_utils::ImmutableBytes> payload)
    : m_payload(std::move(payload))
{
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

constexpr tu_uint32 kMessageVersion1 = 1;
constexpr tu_uint32 kMaxPayloadSize = 16777216;     // 2^24

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
    appender.appendU8(kMessageVersion1);
    appender.appendU32(payloadSize);
    appender.appendBytes(m_payload->getSpan());
    auto bytes = appender.finish();

    return std::static_pointer_cast<const tempo_utils::ImmutableBytes>(bytes);
}

void
chord_mesh::MessageBuilder::reset()
{
    m_payload = {};
}

chord_mesh::MessageParser::MessageParser()
    : m_messageSize(0)
{
}

bool
chord_mesh::MessageParser::appendBytes(std::span<const tu_uint8> bytes)
{
    if (m_pending == nullptr) {
        m_pending = std::make_unique<tempo_utils::BytesAppender>();
    }

    m_pending->appendBytes(bytes);

    if (m_messageSize == 0) {
        if (m_pending->getSize() >= 5) {
            auto *ptr = m_pending->getData();
            m_messageVersion = tempo_utils::read_u8_and_advance(ptr);
            m_messageSize = tempo_utils::read_u32(ptr);
        }
    }

    // we haven't read enough input to get the size
    if (m_messageSize == 0)
        return !m_messages.empty();

    // we haven't read enough input to parse the whole message
    if (m_pending->getSize() - 4 < m_messageSize)
        return !m_messages.empty();

    // finish the appender
    auto pending = m_pending->finish();
    m_pending.reset();

    // extract the payload and add it to messages
    auto slice = pending->toSlice();
    auto payload = slice.slice(5, m_messageSize).toImmutableBytes();
    m_messages.push(payload);

    // if there is additional data then add it to the appender
    auto remainder = slice.slice(m_messageSize + 5, pending->getSize() - (m_messageSize + 5));
    if (!remainder.isEmpty()) {
        m_pending->appendBytes(remainder.sliceView());
    }

    m_messageSize = 0;
    m_messageVersion = 0;

    return true;
}

bool
chord_mesh::MessageParser::hasMessage() const
{
    return !m_messages.empty();
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::MessageParser::takeMessage()
{
    if (m_messages.empty())
        return {};
    auto message = m_messages.front();
    m_messages.pop();
    return message;
}