
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <chord_mesh/handshake.h>
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream_buf.h>
#include <chord_mesh/stream_io.h>

#include "chord_mesh/generated/stream_messages.capnp.h"

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

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::InsecureStreamBehavior::takePending()
{
    return std::static_pointer_cast<const tempo_utils::ImmutableBytes>(m_parser.popPending());
}

chord_mesh::PendingLocalStreamBehavior::PendingLocalStreamBehavior(
    std::span<const tu_uint8> pending,
    std::span<const tu_uint8> remotePublicKey)
    : m_remotePublicKey(remotePublicKey.begin(), remotePublicKey.end())
{
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

std::span<const tu_uint8>
chord_mesh::PendingLocalStreamBehavior::getRemotePublicKey() const
{
    return m_remotePublicKey;
}

chord_mesh::PendingRemoteStreamBehavior::PendingRemoteStreamBehavior(
    std::span<const tu_uint8> pending,
    std::span<const tu_uint8> localPrivateKey)
    : m_localPrivateKey(localPrivateKey.begin(), localPrivateKey.end())
{
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

std::span<const tu_uint8>
chord_mesh::PendingRemoteStreamBehavior::getLocalPrivateKey() const
{
    return m_localPrivateKey;
}

chord_mesh::HandshakingStreamBehavior::HandshakingStreamBehavior(
    std::shared_ptr<Handshake> handshake,
    AbstractStreamBufWriter *writer)
    : m_handshake(std::move(handshake)),
      m_writer(writer)
{
    TU_ASSERT (m_handshake != nullptr);
    TU_ASSERT (m_writer != nullptr);
}

tempo_utils::Status
chord_mesh::HandshakingStreamBehavior::start()
{
    return m_handshake->start();
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
        return status;
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
    std::span<const tu_uint8> remotePublicKey,
    std::string_view pemCertificateString,
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
            m_behavior = std::make_unique<PendingLocalStreamBehavior>(pending->getSpan(), remotePublicKey);
            m_state = IOState::PendingLocal;
            break;
        }
        case IOState::PendingRemote: {
            auto prev = std::move(m_behavior);
            auto *pendingRemote = (PendingRemoteStreamBehavior *) prev.get();
            auto localPrivateKey = pendingRemote->getLocalPrivateKey();
            std::shared_ptr<Handshake> handshake;
            TU_ASSIGN_OR_RETURN (handshake, Handshake::create("Noise_KK_25519_ChaChaPoly_BLAKE2s",
                m_initiator, localPrivateKey, remotePublicKey));
            auto behavior = std::make_unique<HandshakingStreamBehavior>(handshake, m_writer);
            TU_RETURN_IF_NOT_OK (behavior->start());
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
chord_mesh::StreamIO::negotiateLocal(
    std::string_view pemCertificateString,
    const StaticKeypair &localKeypair)
{
    switch (m_state) {
        case IOState::Insecure: {
            auto prev = std::move(m_behavior);
            auto *insecure = (InsecureStreamBehavior *) prev.get();
            auto pending = insecure->takePending();
            m_behavior = std::make_unique<PendingRemoteStreamBehavior>(pending->getSpan(), localKeypair.privateKey);
            m_state = IOState::PendingRemote;
            break;
        }
        case IOState::PendingLocal: {
            auto prev = std::move(m_behavior);
            auto *pendingLocal = (PendingLocalStreamBehavior *) prev.get();
            auto remotePublicKey = pendingLocal->getRemotePublicKey();
            std::shared_ptr<Handshake> handshake;
            TU_ASSIGN_OR_RETURN (handshake, Handshake::create("Noise_KK_25519_ChaChaPoly_BLAKE2s",
                m_initiator, localKeypair.privateKey, remotePublicKey));
            auto behavior = std::make_unique<HandshakingStreamBehavior>(handshake, m_writer);
            TU_RETURN_IF_NOT_OK (behavior->start());
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
