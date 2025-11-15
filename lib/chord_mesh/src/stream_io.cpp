
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <chord_mesh/generated/stream_messages.capnp.h>
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/noise.h>
#include <chord_mesh/stream_buf.h>
#include <chord_mesh/stream_io.h>

chord_mesh::Pending::Pending()
{
}

chord_mesh::Pending::~Pending()
{
    while (!m_outgoing.empty()) {
        auto *outgoing = m_outgoing.front();
        free_stream_buf(outgoing);
        m_outgoing.pop();
    }
}

void
chord_mesh::Pending::pushIncoming(std::span<const tu_uint8> incoming)
{
    if (incoming.empty())
        return;
    if (m_incoming == nullptr) {
        m_incoming = std::make_unique<tempo_utils::BytesAppender>();
    }
    m_incoming->appendBytes(incoming);
}

bool
chord_mesh::Pending::hasIncoming() const
{
    return m_incoming != nullptr;
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::Pending::popIncoming()
{
    if (m_incoming == nullptr)
        return {};
    auto incoming = m_incoming->finish();
    m_incoming.reset();
    return incoming;
}

void
chord_mesh::Pending::pushOutgoing(StreamBuf *streamBuf)
{
    TU_ASSERT (streamBuf != nullptr);
    if (streamBuf->buf.len > 0) {
        m_outgoing.push(streamBuf);
    } else {
        free_stream_buf(streamBuf);
    }
}

bool
chord_mesh::Pending::hasOutgoing() const
{
    return !m_outgoing.empty();
}

chord_mesh::StreamBuf *
chord_mesh::Pending::popOutgoing()
{
    if (m_outgoing.empty())
        return nullptr;
    auto outgoing = m_outgoing.front();
    m_outgoing.pop();
    return outgoing;
}

chord_mesh::InitialStreamBehavior::InitialStreamBehavior()
    : m_pending(std::make_unique<Pending>())
{
}

tempo_utils::Status
chord_mesh::InitialStreamBehavior::read(const tu_uint8 *data, ssize_t size)
{
    return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
        "stream IO cannot read in Initial state");
}

tempo_utils::Status
chord_mesh::InitialStreamBehavior::write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf)
{
    m_pending->pushOutgoing(streamBuf);
    return {};
}

tempo_utils::Status
chord_mesh::InitialStreamBehavior::check(bool &ready)
{
    ready = false;
    return {};
}

tempo_utils::Status
chord_mesh::InitialStreamBehavior::take(Message &message)
{
    return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
        "stream IO cannot take in Initial state");
}

std::unique_ptr<chord_mesh::Pending>&&
chord_mesh::InitialStreamBehavior::takePending()
{
    return std::move(m_pending);
}

chord_mesh::InsecureStreamBehavior::InsecureStreamBehavior(bool secure)
    : m_secure(secure),
      m_pending(std::make_unique<Pending>())
{
}

tempo_utils::Status
chord_mesh::InsecureStreamBehavior::read(const tu_uint8 *data, ssize_t size)
{
    std::span bytes(data, size);
    return m_parser.pushBytes(bytes);
}

tempo_utils::Status
chord_mesh::InsecureStreamBehavior::write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf)
{
    auto status = writer->write(streamBuf);
    if (status.notOk()) {
        free_stream_buf(streamBuf);
    }
    return status;
}

tempo_utils::Status
chord_mesh::InsecureStreamBehavior::check(bool &ready)
{
    return m_parser.checkReady(ready);
}

tempo_utils::Status
chord_mesh::InsecureStreamBehavior::take(Message &message)
{
    Message m;
    TU_RETURN_IF_NOT_OK (m_parser.takeReady(m));

    // if we are operating in secure mode then we only permit stream messages
    if (m_secure && m.getVersion() != MessageVersion::Stream)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "stream IO cannot take message in Insecure state");

    message = m;
    return {};
}

tempo_utils::Status
chord_mesh::InsecureStreamBehavior::negotiate(
    std::shared_ptr<const tempo_utils::ImmutableBytes> bytes,
    AbstractStreamBufWriter *writer)
{
    auto *streamBuf = ImmutableBytesBuf::allocate(bytes);
    return writer->write(streamBuf);
}

tempo_utils::Status
chord_mesh::InsecureStreamBehavior::applyPending(
    std::unique_ptr<Pending> &&pending,
    AbstractStreamBufWriter *writer)
{
    while (pending->hasOutgoing()) {
        auto *outgoing = pending->popOutgoing();
        TU_RETURN_IF_NOT_OK (writer->write(outgoing));
    }
    return {};
}

std::unique_ptr<chord_mesh::Pending>&&
chord_mesh::InsecureStreamBehavior::takePending()
{
    auto bytes = m_parser.popPending();
    m_pending->pushIncoming(bytes->getSpan());
    return std::move(m_pending);
}

chord_mesh::PendingLocalStreamBehavior::PendingLocalStreamBehavior(
    std::string_view protocolName,
    std::span<const tu_uint8> remotePublicKey,
    std::unique_ptr<Pending> &&pending)
    : m_protocolName(protocolName),
      m_remotePublicKey(remotePublicKey.begin(), remotePublicKey.end()),
      m_pending(std::move(pending))
{
    TU_ASSERT (!m_protocolName.empty());
    TU_ASSERT (!m_remotePublicKey.empty());
}

tempo_utils::Status
chord_mesh::PendingLocalStreamBehavior::read(const tu_uint8 *data, ssize_t size)
{
    m_pending->pushIncoming(std::span(data, size));
    return {};
}

tempo_utils::Status
chord_mesh::PendingLocalStreamBehavior::write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf)
{
    m_pending->pushOutgoing(streamBuf);
    return {};
}

tempo_utils::Status
chord_mesh::PendingLocalStreamBehavior::check(bool &ready)
{
    ready = false;
    return {};
}

tempo_utils::Status
chord_mesh::PendingLocalStreamBehavior::take(Message &message)
{
    return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
        "stream IO cannot take in PendingLocal state");
}

std::string
chord_mesh::PendingLocalStreamBehavior::getRemoteProtocolName() const
{
    return m_protocolName;
}

std::span<const tu_uint8>
chord_mesh::PendingLocalStreamBehavior::getRemotePublicKey() const
{
    return m_remotePublicKey;
}

tempo_utils::Status
chord_mesh::PendingLocalStreamBehavior::negotiate(
    std::shared_ptr<const tempo_utils::ImmutableBytes> bytes,
    AbstractStreamBufWriter *writer)
{
    auto *streamBuf = ImmutableBytesBuf::allocate(bytes);
    return writer->write(streamBuf);
}

std::unique_ptr<chord_mesh::Pending>&&
chord_mesh::PendingLocalStreamBehavior::takePending()
{
    return std::move(m_pending);
}

chord_mesh::PendingRemoteStreamBehavior::PendingRemoteStreamBehavior(
    std::string_view protocolName,
    std::span<const tu_uint8> localPrivateKey,
    std::unique_ptr<Pending> &&pending)
    : m_protocolName(protocolName),
      m_localPrivateKey(localPrivateKey.begin(), localPrivateKey.end()),
      m_pending(std::move(pending))
{
    TU_ASSERT (!m_protocolName.empty());
    TU_ASSERT (!m_localPrivateKey.empty());
}

tempo_utils::Status
chord_mesh::PendingRemoteStreamBehavior::read(const tu_uint8 *data, ssize_t size)
{
    std::span bytes(data, size);
    return m_parser.pushBytes(bytes);
}

tempo_utils::Status
chord_mesh::PendingRemoteStreamBehavior::write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf)
{
    m_pending->pushOutgoing(streamBuf);
    return {};
}

tempo_utils::Status
chord_mesh::PendingRemoteStreamBehavior::check(bool &ready)
{
    return m_parser.checkReady(ready);
}

tempo_utils::Status
chord_mesh::PendingRemoteStreamBehavior::take(Message &message)
{
    return m_parser.takeReady(message);
}

std::string
chord_mesh::PendingRemoteStreamBehavior::getLocalProtocolName() const
{
    return m_protocolName;
}

std::span<const tu_uint8>
chord_mesh::PendingRemoteStreamBehavior::getLocalPrivateKey() const
{
    return m_localPrivateKey;
}

std::unique_ptr<chord_mesh::Pending>&&
chord_mesh::PendingRemoteStreamBehavior::takePending()
{
    return std::move(m_pending);
}

chord_mesh::HandshakingStreamBehavior::HandshakingStreamBehavior(
    std::shared_ptr<Handshake> handshake,
    std::unique_ptr<Pending> &&pending)
    : m_handshake(std::move(handshake)),
      m_pending(std::move(pending))
{
    TU_ASSERT (m_handshake != nullptr);
}

inline tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>>
build_stream_handshake_message(std::span<const tu_uint8> data)
{
    // construct the StreamHandshake payload
    ::capnp::MallocMessageBuilder capnpBuilder;
    chord_mesh::generated::StreamMessage::Builder root = capnpBuilder.initRoot<chord_mesh::generated::StreamMessage>();
    auto streamHandshake = root.initMessage().initStreamHandshake();
    streamHandshake.setData(capnp::Data::Reader(data.data(), data.size()));

    // serialize the payload
    auto flatArray = capnp::messageToFlatArray(capnpBuilder);
    auto arrayPtr = flatArray.asBytes();

    // construct the message
    chord_mesh::MessageBuilder messageBuilder;
    messageBuilder.setVersion(chord_mesh::MessageVersion::Stream);
    messageBuilder.setPayload(tempo_utils::MemoryBytes::copy(std::span(arrayPtr.begin(), arrayPtr.end())));
    return messageBuilder.toBytes();
}

tempo_utils::Status
chord_mesh::HandshakingStreamBehavior::start(AbstractStreamBufWriter *writer)
{
    TU_RETURN_IF_NOT_OK (m_handshake->start());

    // send outgoing handshake messages
    while (m_handshake->hasOutgoing()) {
        auto outgoing = m_handshake->popOutgoing();

        std::shared_ptr<const tempo_utils::ImmutableBytes> payload;
        TU_ASSIGN_OR_RETURN (payload, build_stream_handshake_message(outgoing->getSpan()));

        auto *streamBuf = ImmutableBytesBuf::allocate(payload);
        auto status = writer->write(streamBuf);

        if (status.notOk()) {
            free_stream_buf(streamBuf);
        }
        return status;
    }

    return {};
}

tempo_utils::Status
chord_mesh::HandshakingStreamBehavior::process(
    std::span<const tu_uint8> data,
    AbstractStreamBufWriter *writer,
    bool &finished)
{
    TU_RETURN_IF_NOT_OK (m_handshake->process(data.data(), data.size()));

    // send outgoing handshake messages
    while (m_handshake->hasOutgoing()) {
        auto outgoing = m_handshake->popOutgoing();

        std::shared_ptr<const tempo_utils::ImmutableBytes> payload;
        TU_ASSIGN_OR_RETURN (payload, build_stream_handshake_message(outgoing->getSpan()));

        auto *streamBuf = ImmutableBytesBuf::allocate(payload);
        auto status = writer->write(streamBuf);

        if (status.notOk()) {
            free_stream_buf(streamBuf);
        }
    }

    // if the handshake has completed (successfully or not) then signal finished
    switch (m_handshake->getHandshakeState()) {
        case HandshakeState::Split:
        case HandshakeState::Failed:
            finished = true;
            break;
        default:
            finished = false;
            break;
    }

    return {};
}

tempo_utils::Result<std::shared_ptr<chord_mesh::Cipher>>
chord_mesh::HandshakingStreamBehavior::finish()
{
    std::shared_ptr<Cipher> cipher;
    TU_ASSIGN_OR_RETURN (cipher, m_handshake->finish());

    if (m_parser.hasPending()) {
        auto pending = m_parser.popPending();
        TU_RETURN_IF_NOT_OK (cipher->decryptInput(pending->getData(), pending->getSize()));
    }
    return cipher;
}

std::unique_ptr<chord_mesh::Pending>&&
chord_mesh::HandshakingStreamBehavior::takePending()
{
    return std::move(m_pending);
}

tempo_utils::Status
chord_mesh::HandshakingStreamBehavior::read(const tu_uint8 *data, ssize_t size)
{
    std::span bytes(data, size);
    return m_parser.pushBytes(bytes);
}

tempo_utils::Status
chord_mesh::HandshakingStreamBehavior::write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf)
{
    return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
        "stream IO cannot write in Handshaking state");
}

tempo_utils::Status
chord_mesh::HandshakingStreamBehavior::check(bool &ready)
{
    return m_parser.checkReady(ready);
}

tempo_utils::Status
chord_mesh::HandshakingStreamBehavior::take(Message &message)
{
    Message ready;
    TU_RETURN_IF_NOT_OK (m_parser.takeReady(ready));

    if (ready.getVersion() != MessageVersion::Stream)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid message while in Handshaking state");
    message = ready;
    return {};
}

chord_mesh::SecureStreamBehavior::SecureStreamBehavior(
    std::shared_ptr<Cipher> cipher,
    std::unique_ptr<Pending> &&pending)
    : m_cipher(std::move(cipher)),
      m_pending(std::move(pending))
{
    TU_ASSERT (m_cipher != nullptr);
}

tempo_utils::Status
chord_mesh::SecureStreamBehavior::start(AbstractStreamBufWriter *writer)
{
    if (m_pending->hasIncoming())
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "unexpected incoming data when starting Secure state");

    while (m_pending->hasOutgoing()) {
        auto *outgoing = m_pending->popOutgoing();
        TU_RETURN_IF_NOT_OK (m_cipher->encryptOutput(outgoing));
    }

    while (m_cipher->hasOutput()) {
        auto output = m_cipher->popOutput();
        auto status = writer->write(output);
        if (status.notOk()) {
            free_stream_buf(output);
            return status;
        }
    }

    return {};
}

tempo_utils::Status
chord_mesh::SecureStreamBehavior::read(const tu_uint8 *data, ssize_t size)
{
    TU_RETURN_IF_NOT_OK (m_cipher->decryptInput(data, size));
    while (m_cipher->hasInput()) {
        auto input = m_cipher->popInput();
        TU_RETURN_IF_NOT_OK (m_parser.pushBytes(input->getSpan()));
    }
    return {};
}

tempo_utils::Status
chord_mesh::SecureStreamBehavior::write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf)
{
    TU_RETURN_IF_NOT_OK (m_cipher->encryptOutput(streamBuf));
    while (m_cipher->hasOutput()) {
        auto output = m_cipher->popOutput();
        auto status = writer->write(output);
        if (status.notOk()) {
            free_stream_buf(output);
            return status;
        }
    }
    return {};
}

tempo_utils::Status
chord_mesh::SecureStreamBehavior::check(bool &ready)
{
    return m_parser.checkReady(ready);
}

tempo_utils::Status
chord_mesh::SecureStreamBehavior::take(Message &message)
{
    return m_parser.takeReady(message);
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::SecureStreamBehavior::takePending()
{
    return m_parser.popPending();
}

chord_mesh::StreamIO::StreamIO(bool initiator, StreamManager *manager, AbstractStreamBufWriter *writer)
    : m_initiator(initiator),
      m_manager(manager),
      m_writer(writer),
      m_state(IOState::Initial),
      m_behavior(std::make_unique<InitialStreamBehavior>())
{
    TU_ASSERT (m_manager != nullptr);
    TU_ASSERT (m_writer != nullptr);
}

chord_mesh::IOState
chord_mesh::StreamIO::getIOState() const
{
    return m_state;
}

tempo_utils::Status
chord_mesh::StreamIO::start(bool secure)
{
    if (m_state != IOState::Initial)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream IO state");

    auto prev = std::move(m_behavior);
    auto *initial = (InitialStreamBehavior *) prev.get();
    auto pending = initial->takePending();

    m_behavior = std::make_unique<InsecureStreamBehavior>(secure);
    auto *insecure = (InsecureStreamBehavior *) m_behavior.get();

    while (pending->hasOutgoing()) {
        auto *streamBuf = pending->popOutgoing();
        TU_RETURN_IF_NOT_OK (insecure->write(m_writer, streamBuf));
    }

    m_state = IOState::Insecure;

    TU_LOG_V << "stream " << this << " moves from Initial to Insecure state";

    return {};
}

inline tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>>
build_stream_negotiate_message(
    std::string_view protocolName,
    std::span<const tu_uint8> publicKey,
    std::string_view certificate,
    const tempo_security::Digest &digest)
{
    // construct the StreamNegotiate payload
    ::capnp::MallocMessageBuilder capnpBuilder;
    chord_mesh::generated::StreamMessage::Builder root = capnpBuilder.initRoot<chord_mesh::generated::StreamMessage>();
    auto streamNegotiate = root.initMessage().initStreamNegotiate();
    streamNegotiate.setProtocol(std::string(protocolName));
    streamNegotiate.setPublicKey(capnp::Data::Reader(publicKey.data(), publicKey.size()));
    streamNegotiate.setCertificate(std::string(certificate));
    streamNegotiate.setDigest(capnp::Data::Reader(digest.getData(), digest.getSize()));

    // serialize the payload
    auto flatArray = capnp::messageToFlatArray(capnpBuilder);
    auto arrayPtr = flatArray.asBytes();

    // construct the message
    chord_mesh::MessageBuilder messageBuilder;
    messageBuilder.setVersion(chord_mesh::MessageVersion::Stream);
    messageBuilder.setPayload(tempo_utils::MemoryBytes::copy(std::span(arrayPtr.begin(), arrayPtr.end())));
    return messageBuilder.toBytes();
}

tempo_utils::Status
chord_mesh::StreamIO::negotiateRemote(
    std::string_view protocolName,
    std::string_view pemCertificateString,
    std::span<const tu_uint8> remotePublicKey,
    const tempo_security::Digest &digest)
{
    // validate the public key is signed by a trusted certificate
    std::shared_ptr<tempo_security::X509Certificate> certificate;
    TU_ASSIGN_OR_RETURN (certificate, tempo_security::X509Certificate::fromString(pemCertificateString));
    auto trustStore = m_manager->getTrustStore();
    TU_RETURN_IF_NOT_OK (validate_static_key(remotePublicKey, certificate, trustStore, digest));

    switch (m_state) {

        case IOState::Insecure: {
            auto prev = std::move(m_behavior);
            auto *insecure = (InsecureStreamBehavior *) prev.get();
            auto pending = insecure->takePending();
            m_behavior = std::make_unique<PendingLocalStreamBehavior>(
                protocolName, remotePublicKey, std::move(pending));
            m_state = IOState::PendingLocal;
            TU_LOG_V << "stream " << this << " moves from Insecure to PendingLocal state";
            break;
        }

        case IOState::PendingRemote: {
            auto prev = std::move(m_behavior);
            auto *pendingRemote = (PendingRemoteStreamBehavior *) prev.get();
            auto localPrivateKey = pendingRemote->getLocalPrivateKey();
            auto pending = pendingRemote->takePending();

            if (protocolName != pendingRemote->getLocalProtocolName())
                return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                    "remote protocol '{}' does not match local protocol '{}'",
                    protocolName, pendingRemote->getLocalProtocolName());

            std::shared_ptr<Handshake> handshake;
            TU_ASSIGN_OR_RETURN (handshake, Handshake::create(
                protocolName, m_initiator, localPrivateKey, remotePublicKey));
            auto behavior = std::make_unique<HandshakingStreamBehavior>(handshake, std::move(pending));
            TU_RETURN_IF_NOT_OK (behavior->start(m_writer));

            m_behavior = std::move(behavior);
            m_state = IOState::Handshaking;
            TU_LOG_V << "stream " << this << " moves from PendingRemote to Handshaking state";
            break;
        }

        default:
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "invalid stream IO state");
    }

    return {};
}

tempo_utils::Status
chord_mesh::StreamIO::negotiateLocal(
    std::string_view protocolName,
    std::string_view pemCertificateString,
    const StaticKeypair &localKeypair)
{
    switch (m_state) {

        case IOState::Insecure: {
            auto prev = std::move(m_behavior);
            auto *insecure = (InsecureStreamBehavior *) prev.get();
            std::shared_ptr<const tempo_utils::ImmutableBytes> negotiateBytes;
            TU_ASSIGN_OR_RETURN (negotiateBytes, build_stream_negotiate_message(
                protocolName, localKeypair.publicKey, pemCertificateString, localKeypair.digest));
            TU_RETURN_IF_NOT_OK (insecure->negotiate(negotiateBytes, m_writer));
            auto pending = insecure->takePending();
            m_behavior = std::make_unique<PendingRemoteStreamBehavior>(
                protocolName, localKeypair.privateKey, std::move(pending));
            m_state = IOState::PendingRemote;
            TU_LOG_V << "stream " << this << " moves from Insecure to PendingRemote state";
            break;
        }

        case IOState::PendingLocal: {
            auto prev = std::move(m_behavior);
            auto *pendingLocal = (PendingLocalStreamBehavior *) prev.get();
            auto remotePublicKey = pendingLocal->getRemotePublicKey();
            auto pending = pendingLocal->takePending();

            std::shared_ptr<const tempo_utils::ImmutableBytes> negotiateBytes;
            TU_ASSIGN_OR_RETURN (negotiateBytes, build_stream_negotiate_message(
                protocolName, localKeypair.publicKey, pemCertificateString, localKeypair.digest));
            TU_RETURN_IF_NOT_OK (pendingLocal->negotiate(negotiateBytes, m_writer));

            if (protocolName != pendingLocal->getRemoteProtocolName())
                return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                    "local protocol '{}' does not match remote protocol '{}'",
                    protocolName, pendingLocal->getRemoteProtocolName());

            std::shared_ptr<Handshake> handshake;
            TU_ASSIGN_OR_RETURN (handshake, Handshake::create(
                protocolName, m_initiator, localKeypair.privateKey, remotePublicKey));
            auto behavior = std::make_unique<HandshakingStreamBehavior>(handshake, std::move(pending));
            TU_RETURN_IF_NOT_OK (behavior->start(m_writer));

            m_behavior = std::move(behavior);
            m_state = IOState::Handshaking;
            TU_LOG_V << "stream " << this << " moves from PendingLocal to Handshaking state";
            break;
        }

        default:
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "invalid stream IO state");
    }

    return {};
}

tempo_utils::Status
chord_mesh::StreamIO::processHandshake(std::span<const tu_uint8> data, bool &finished)
{
    if (m_state != IOState::Handshaking)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream IO state");
    auto *handshaking = (HandshakingStreamBehavior *) m_behavior.get();
    auto pending = handshaking->takePending();

    TU_LOG_V << "stream " << this << " received " << (int) data.size() << " bytes of handshake data";

    TU_RETURN_IF_NOT_OK (handshaking->process(data, m_writer, finished));
    if (!finished)
        return {};

    std::shared_ptr<Cipher> cipher;
    TU_ASSIGN_OR_RETURN (cipher, handshaking->finish());

    auto secure = std::make_unique<SecureStreamBehavior>(cipher, std::move(pending));
    TU_RETURN_IF_NOT_OK (secure->start(m_writer));
    m_behavior = std::move(secure);
    m_state = IOState::Secure;

    TU_LOG_V << "stream " << this << " moves from Handshaking to Secure state";

    return {};
}

tempo_utils::Status
chord_mesh::StreamIO::read(const tu_uint8 *data, ssize_t size)
{
    if (m_behavior == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream IO state");
    return m_behavior->read(data, size);
}

tempo_utils::Status
chord_mesh::StreamIO::write(StreamBuf *streamBuf)
{
    if (m_behavior == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream IO state");
    return m_behavior->write(m_writer, streamBuf);
}

tempo_utils::Status
chord_mesh::StreamIO::write(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes)
{
    auto *streamBuf = ImmutableBytesBuf::allocate(std::move(bytes));
    auto status = write(streamBuf);
    if (status.notOk()) {
        free_stream_buf(streamBuf);
    }
    return status;
}

tempo_utils::Status
chord_mesh::StreamIO::checkReady(bool &ready)
{
    if (m_behavior == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream IO state");
    return m_behavior->check(ready);
}

tempo_utils::Status
chord_mesh::StreamIO::takeReady(Message &message)
{
    if (m_behavior == nullptr)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream IO state");
    return m_behavior->take(message);
}
