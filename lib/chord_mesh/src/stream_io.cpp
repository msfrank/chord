
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <chord_mesh/generated/stream_messages.capnp.h>
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/noise.h>
#include <chord_mesh/stream_buf.h>
#include <chord_mesh/stream_io.h>

tempo_utils::Status
chord_mesh::InitialStreamBehavior::read(const tu_uint8 *data, ssize_t size)
{
    return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
        "stream IO cannot read in Initial state");
}

tempo_utils::Status
chord_mesh::InitialStreamBehavior::write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf)
{
    m_outgoing.push(streamBuf);
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

bool
chord_mesh::InitialStreamBehavior::hasOutgoing() const
{
    return !m_outgoing.empty();
}

chord_mesh::StreamBuf *
chord_mesh::InitialStreamBehavior::takeOutgoing()
{
    if (m_outgoing.empty())
        return nullptr;
    auto outgoing = m_outgoing.front();
    m_outgoing.pop();
    return outgoing;
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
    return m_parser.takeReady(message);
}

tempo_utils::Status
chord_mesh::InsecureStreamBehavior::negotiate(
    std::shared_ptr<const tempo_utils::ImmutableBytes> bytes,
    AbstractStreamBufWriter *writer)
{
    auto *streamBuf = ImmutableBytesBuf::allocate(bytes);
    return writer->write(streamBuf);
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::InsecureStreamBehavior::takePending()
{
    return std::static_pointer_cast<const tempo_utils::ImmutableBytes>(m_parser.popPending());
}

chord_mesh::PendingLocalStreamBehavior::PendingLocalStreamBehavior(
    std::string_view protocolName,
    std::span<const tu_uint8> remotePublicKey,
    std::span<const tu_uint8> pending)
    : m_protocolName(protocolName),
      m_remotePublicKey(remotePublicKey.begin(), remotePublicKey.end())
{
    TU_ASSERT (!m_protocolName.empty());
    TU_ASSERT (!m_remotePublicKey.empty());
    m_pending.appendBytes(pending);
}

tempo_utils::Status
chord_mesh::PendingLocalStreamBehavior::read(const tu_uint8 *data, ssize_t size)
{
    m_pending.appendBytes(std::span(data, size));
    return {};
}

tempo_utils::Status
chord_mesh::PendingLocalStreamBehavior::write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf)
{
    return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
        "stream IO cannot write in PendingLocal state");
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

chord_mesh::PendingRemoteStreamBehavior::PendingRemoteStreamBehavior(
    std::string_view protocolName,
    std::span<const tu_uint8> localPrivateKey,
    std::span<const tu_uint8> pending)
    : m_protocolName(protocolName),
      m_localPrivateKey(localPrivateKey.begin(), localPrivateKey.end())
{
    TU_ASSERT (!m_protocolName.empty());
    TU_ASSERT (!m_localPrivateKey.empty());
    m_pending.appendBytes(pending);
}

tempo_utils::Status
chord_mesh::PendingRemoteStreamBehavior::read(const tu_uint8 *data, ssize_t size)
{
    m_pending.appendBytes(std::span(data, size));
    return {};
}

tempo_utils::Status
chord_mesh::PendingRemoteStreamBehavior::write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf)
{
    return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
        "stream IO cannot write in PendingRemote state");
}

tempo_utils::Status
chord_mesh::PendingRemoteStreamBehavior::check(bool &ready)
{
    ready = false;
    return {};
}

tempo_utils::Status
chord_mesh::PendingRemoteStreamBehavior::take(Message &message)
{
    return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
        "stream IO cannot take in PendingRemote state");
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

chord_mesh::HandshakingStreamBehavior::HandshakingStreamBehavior(std::shared_ptr<Handshake> handshake)
    : m_handshake(std::move(handshake))
{
    TU_ASSERT (m_handshake != nullptr);
}

tempo_utils::Status
chord_mesh::HandshakingStreamBehavior::start(AbstractStreamBufWriter *writer)
{
    TU_RETURN_IF_NOT_OK (m_handshake->start());

    // send outgoing handshake messages
    while (m_handshake->hasOutgoing()) {
        auto outgoing = m_handshake->popOutgoing();

        // construct the StreamHandshake message
        ::capnp::MallocMessageBuilder message;
        generated::StreamMessage::Builder root = message.initRoot<generated::StreamMessage>();
        auto streamHandshake = root.initMessage().initStreamHandshake();
        streamHandshake.setData(capnp::Data::Reader(outgoing->getData(), outgoing->getSize()));

        // send the message
        auto flatArray = capnp::messageToFlatArray(message);
        auto arrayPtr = flatArray.asBytes();
        auto *streamBuf = ArrayBuf::allocate(arrayPtr);
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

        // construct the StreamHandshake message
        ::capnp::MallocMessageBuilder message;
        generated::StreamMessage::Builder root = message.initRoot<generated::StreamMessage>();
        auto streamHandshake = root.initMessage().initStreamHandshake();
        streamHandshake.setData(capnp::Data::Reader(outgoing->getData(), outgoing->getSize()));

        // send the message
        auto flatArray = capnp::messageToFlatArray(message);
        auto arrayPtr = flatArray.asBytes();
        auto *streamBuf = ArrayBuf::allocate(arrayPtr);
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
    return m_handshake->finish();
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::HandshakingStreamBehavior::takePending()
{
    return m_parser.popPending();
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

chord_mesh::SecureStreamBehavior::SecureStreamBehavior(std::shared_ptr<Cipher> cipher)
    : m_cipher(std::move(cipher))
{
    TU_ASSERT (m_cipher != nullptr);
}

tempo_utils::Status
chord_mesh::SecureStreamBehavior::start(std::span<const tu_uint8> pending)
{
    return m_cipher->decryptInput(pending.data(), pending.size());
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
chord_mesh::StreamIO::start()
{
    if (m_state != IOState::Initial)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream IO state");

    auto prev = std::move(m_behavior);
    auto *initial = (InitialStreamBehavior *) prev.get();

    m_behavior = std::make_unique<InsecureStreamBehavior>();
    auto *insecure = (InsecureStreamBehavior *) m_behavior.get();

    while (initial->hasOutgoing()) {
        auto *streamBuf = initial->takeOutgoing();
        TU_RETURN_IF_NOT_OK (insecure->write(m_writer, streamBuf));
    }

    m_state = IOState::Insecure;

    return {};
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
                protocolName, remotePublicKey, pending->getSpan());
            m_state = IOState::PendingLocal;
            break;
        }

        case IOState::PendingRemote: {
            auto prev = std::move(m_behavior);
            auto *pendingRemote = (PendingRemoteStreamBehavior *) prev.get();
            auto localPrivateKey = pendingRemote->getLocalPrivateKey();

            if (protocolName != pendingRemote->getLocalProtocolName())
                return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                    "remote protocol '{}' does not match local protocol '{}'",
                    protocolName, pendingRemote->getLocalProtocolName());

            std::shared_ptr<Handshake> handshake;
            TU_ASSIGN_OR_RETURN (handshake, Handshake::create(
                protocolName, m_initiator, localPrivateKey, remotePublicKey));
            auto behavior = std::make_unique<HandshakingStreamBehavior>(handshake);
            TU_RETURN_IF_NOT_OK (behavior->start(m_writer));

            m_behavior = std::move(behavior);
            m_state = IOState::Handshaking;
            break;
        }

        default:
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "invalid stream IO state");
    }

    return {};
}

inline tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>>
build_stream_negotiate_message(
    std::string_view protocolName,
    std::span<const tu_uint8> publicKey,
    std::string_view certificate,
    const tempo_security::Digest &digest)
{
    // construct the StreamNegotiate message
    ::capnp::MallocMessageBuilder capnpBuilder;
    chord_mesh::generated::StreamMessage::Builder root = capnpBuilder.initRoot<chord_mesh::generated::StreamMessage>();
    auto streamNegotiate = root.initMessage().initStreamNegotiate();
    streamNegotiate.setProtocol(std::string(protocolName));
    streamNegotiate.setPublicKey(capnp::Data::Reader(publicKey.data(), publicKey.size()));
    streamNegotiate.setCertificate(std::string(certificate));
    streamNegotiate.setDigest(capnp::Data::Reader(digest.getData(), digest.getSize()));

    // serialize the message
    auto flatArray = capnp::messageToFlatArray(capnpBuilder);
    auto arrayPtr = flatArray.asBytes();

    chord_mesh::MessageBuilder messageBuilder;
    messageBuilder.setVersion(chord_mesh::MessageVersion::Stream);
    messageBuilder.setPayload(tempo_utils::MemoryBytes::copy(std::span(arrayPtr.begin(), arrayPtr.end())));
    return messageBuilder.toBytes();
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
                protocolName, localKeypair.privateKey, pending->getSpan());
            m_state = IOState::PendingRemote;
            break;
        }

        case IOState::PendingLocal: {
            auto prev = std::move(m_behavior);
            auto *pendingLocal = (PendingLocalStreamBehavior *) prev.get();
            auto remotePublicKey = pendingLocal->getRemotePublicKey();
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
            auto behavior = std::make_unique<HandshakingStreamBehavior>(handshake);
            TU_RETURN_IF_NOT_OK (behavior->start(m_writer));

            m_behavior = std::move(behavior);
            m_state = IOState::Handshaking;
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

    TU_RETURN_IF_NOT_OK (handshaking->process(data, m_writer, finished));
    if (!finished)
        return {};

    std::shared_ptr<Cipher> cipher;
    TU_ASSIGN_OR_RETURN (cipher, handshaking->finish());
    auto pending = handshaking->takePending();

    auto secure = std::make_unique<SecureStreamBehavior>(cipher);
    TU_RETURN_IF_NOT_OK (secure->start(pending->getSpan()));
    m_behavior = std::move(secure);
    m_state = IOState::Secure;

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
