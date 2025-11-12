
#include <chord_mesh/handshake.h>
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/supervisor_node.h>
#include <tempo_utils/big_endian.h>

chord_mesh::Handshake::Handshake()
    : m_state(HandshakeState::Initial),
      m_handshake(nullptr)
{
}

chord_mesh::Handshake::~Handshake()
{
    if (m_handshake != nullptr) {
        noise_handshakestate_free(m_handshake);
    }
}

inline tempo_utils::Status
noise_error_to_status(int err)
{
    char buf[512];
    noise_strerror(err, buf, 512);
    return chord_mesh::MeshStatus::forCondition(
        chord_mesh::MeshCondition::kMeshInvariant, buf);
}

tempo_utils::Status
chord_mesh::Handshake::initialize(
    const NoiseProtocolId *protocolId,
    int role,
    std::span<const tu_uint8> prologue,
    std::span<const tu_uint8> localPrivateKey,
    std::span<const tu_uint8> remotePublicKey)
{
    TU_ASSERT (m_handshake == nullptr);
    auto ret = noise_handshakestate_new_by_id(&m_handshake, protocolId, role);
    if (ret != NOISE_ERROR_NONE)
        return noise_error_to_status(ret);

    // if a prologue was specified then set it
    if (!prologue.empty()) {
        ret = noise_handshakestate_set_prologue(m_handshake, prologue.data(), prologue.size());
        if (ret != NOISE_ERROR_NONE)
            return noise_error_to_status(ret);
    }

    // set the local keypair if required
    if (noise_handshakestate_needs_local_keypair(m_handshake)) {
        if (localPrivateKey.empty())
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "protocol requires local private key");
        auto *dh = noise_handshakestate_get_local_keypair_dh(m_handshake);
        ret = noise_dhstate_set_keypair_private(dh, localPrivateKey.data(), localPrivateKey.size());
        if (ret != NOISE_ERROR_NONE)
            return noise_error_to_status(ret);
    }

    if (noise_handshakestate_needs_remote_public_key(m_handshake)) {
        if (remotePublicKey.empty())
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "protocol requires remote public key");
        auto *dh = noise_handshakestate_get_remote_public_key_dh(m_handshake);
        ret = noise_dhstate_set_public_key(dh, remotePublicKey.data(), remotePublicKey.size());
        if (ret != NOISE_ERROR_NONE)
            return noise_error_to_status(ret);
    }

    return {};
}

tempo_utils::Result<std::shared_ptr<chord_mesh::Handshake>>
chord_mesh::Handshake::forInitiator(
    const NoiseProtocolId *protocolId,
    std::span<const tu_uint8> localPrivateKey,
    std::span<const tu_uint8> remotePublicKey,
    std::span<const tu_uint8> prologue)
{
    auto initiator = std::shared_ptr<Handshake>(new Handshake());
    TU_RETURN_IF_NOT_OK (initiator->initialize(
        protocolId, NOISE_ROLE_INITIATOR, prologue, localPrivateKey, remotePublicKey));
    return initiator;
}

tempo_utils::Result<std::shared_ptr<chord_mesh::Handshake>>
chord_mesh::Handshake::forResponder(
    const NoiseProtocolId *protocolId,
    std::span<const tu_uint8> localPrivateKey,
    std::span<const tu_uint8> remotePublicKey,
    std::span<const tu_uint8> prologue)
{
    auto initiator = std::shared_ptr<Handshake>(new Handshake());
    TU_RETURN_IF_NOT_OK (initiator->initialize(
        protocolId, NOISE_ROLE_RESPONDER, prologue, localPrivateKey, remotePublicKey));
    return initiator;
}

tempo_utils::Result<std::shared_ptr<chord_mesh::Handshake>>
chord_mesh::Handshake::create(
    std::string_view protocolName,
    bool initiator,
    std::span<const tu_uint8> localPrivateKey,
    std::span<const tu_uint8> remotePublicKey,
    std::span<const tu_uint8> prologue)
{
    NoiseProtocolId protocolId;
    auto ret = noise_protocol_name_to_id(&protocolId, protocolName.data(), protocolName.size());
    if (ret != NOISE_ERROR_NONE)
        return noise_error_to_status(ret);
    if (initiator) {
        return forInitiator(&protocolId, localPrivateKey, remotePublicKey, prologue);
    }
    return forResponder(&protocolId, localPrivateKey, remotePublicKey, prologue);
}

static tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>>
write_handshake_message(NoiseHandshakeState *handshake)
{
    NoiseBuffer buf;
    int ret;

    std::vector<tu_uint8> message(512);
    noise_buffer_set_output(buf, message.data(), message.size());
    ret = noise_handshakestate_write_message(handshake, &buf, nullptr);
    if (ret != NOISE_ERROR_NONE)
        return noise_error_to_status(ret);
    message.resize(buf.size);
    auto bytes = tempo_utils::MemoryBytes::create(std::move(message));
    return std::static_pointer_cast<const tempo_utils::ImmutableBytes>(bytes);
}

tempo_utils::Status
chord_mesh::Handshake::start()
{
    if (m_state != HandshakeState::Initial)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid handshake state");

    auto ret = noise_handshakestate_start(m_handshake);
    if (ret != NOISE_ERROR_NONE)
        return noise_error_to_status(ret);

    while (m_state != HandshakeState::Waiting) {
        auto action = noise_handshakestate_get_action(m_handshake);

        switch (action) {
            case NOISE_ACTION_READ_MESSAGE:
                m_state = HandshakeState::Waiting;
                break;
            case NOISE_ACTION_WRITE_MESSAGE: {
                std::shared_ptr<const tempo_utils::ImmutableBytes> outgoing;
                TU_ASSIGN_OR_RETURN (outgoing, write_handshake_message(m_handshake));
                m_outgoing.push(std::move(outgoing));
                break;
            }
            default:
                m_state = HandshakeState::Failed;
                return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                    "invalid handshake action");
        }
    }

    return {};
}

tempo_utils::Status
chord_mesh::Handshake::process(const tu_uint8 *data, size_t size)
{
    NoiseBuffer buf;
    int ret;

    if (m_state != HandshakeState::Waiting)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid handshake state");

    noise_buffer_set_input(buf, (tu_uint8 *) data, size);
    ret = noise_handshakestate_read_message(m_handshake, &buf, nullptr);
    if (ret != NOISE_ERROR_NONE)
        return noise_error_to_status(ret);

    bool done = false;
    do {
        auto action = noise_handshakestate_get_action(m_handshake);
        switch (action) {
            case NOISE_ACTION_READ_MESSAGE:
                done = true;
                break;
            case NOISE_ACTION_WRITE_MESSAGE: {
                std::shared_ptr<const tempo_utils::ImmutableBytes> outgoing;
                TU_ASSIGN_OR_RETURN (outgoing, write_handshake_message(m_handshake));
                m_outgoing.push(std::move(outgoing));
                break;
            }
            case NOISE_ACTION_SPLIT: {
                m_state = HandshakeState::Split;
                return {};
            }
            case NOISE_ACTION_FAILED: {
                m_state = HandshakeState::Failed;
                return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                    "handshake failure");
            }
            default:
                m_state = HandshakeState::Failed;
                return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                    "invalid handshake action");
        }
    } while (!done);

    return {};
}

tempo_utils::Result<std::shared_ptr<chord_mesh::Cipher>>
chord_mesh::Handshake::finish()
{
    if (m_state != HandshakeState::Split)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid handshake state");
    auto cipher = std::shared_ptr<Cipher>(new Cipher());
    TU_RETURN_IF_NOT_OK (cipher->initialize(m_handshake));
    return cipher;
}

chord_mesh::HandshakeState
chord_mesh::Handshake::getHandshakeState() const
{
    return m_state;
}

bool
chord_mesh::Handshake::hasOutgoing() const
{
    return !m_outgoing.empty();
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::Handshake::popOutgoing()
{
    if (m_outgoing.empty())
        return {};
    auto outgoing = m_outgoing.front();
    m_outgoing.pop();
    return outgoing;
}

chord_mesh::Cipher::Cipher()
    : m_send(nullptr),
      m_recv(nullptr),
      m_pending(std::make_unique<tempo_utils::BytesAppender>())
{
}

chord_mesh::Cipher::~Cipher()
{
    if (m_send != nullptr) {
        noise_cipherstate_free(m_send);
    }
    if (m_recv != nullptr) {
        noise_cipherstate_free(m_recv);
    }
    while (!m_output.empty()) {
        auto *streamBuf = m_output.front();
        m_output.pop();
        free_stream_buf(streamBuf);
    }
}

tempo_utils::Status
chord_mesh::Cipher::initialize(NoiseHandshakeState *handshake)
{
    auto ret = noise_handshakestate_split(handshake, &m_send, &m_recv);
    if (ret != NOISE_ERROR_NONE)
        return noise_error_to_status(ret);
    return {};
}

tempo_utils::Status
chord_mesh::Cipher::decryptInput(const tu_uint8 *data, size_t size)
{
    m_pending->appendBytes(std::span(data, size));

    for (;;) {
        auto pendingSize = m_pending->getSize();
        if (pendingSize < 2)
            break;
        auto *ptr = m_pending->getData();
        tu_uint16 messageSize = tempo_utils::read_u16_and_advance(ptr);
        if (pendingSize < messageSize + 2)
            break;

        auto pending = m_pending->finish()->toSlice();
        m_pending = std::make_unique<tempo_utils::BytesAppender>();

        auto message = pending.slice(2, messageSize);
        auto remainder = pending.slice(messageSize + 2, pending.getSize() - (messageSize + 2));
        m_pending->appendBytes(remainder.getSpan());

        NoiseBuffer buffer;
        noise_buffer_set_input(buffer, (tu_uint8 *) message.getData(), message.getSize());
        auto ret = noise_cipherstate_decrypt(m_recv, &buffer);
        if (ret != NOISE_ERROR_NONE)
            return noise_error_to_status(ret);

        auto input = tempo_utils::MemoryBytes::copy(std::span(buffer.data, buffer.size));
        m_input.push(std::move(input));
    }

    return {};
}

bool
chord_mesh::Cipher::hasInput() const
{
    return !m_input.empty();
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
chord_mesh::Cipher::popInput()
{
    if (m_input.empty())
        return {};
    auto input = m_input.front();
    m_input.pop();
    return input;
}

constexpr int kBufMaxSize = 8192;
constexpr int kBufSegSize = 4096;

tempo_utils::Status
chord_mesh::Cipher::encryptOutput(StreamBuf *streamBuf)
{
    NoiseBuffer buffer;
    std::queue<StreamBuf *> output;
    int ret;

    auto *base = streamBuf->buf.base;
    auto size = streamBuf->buf.len;
    int index = 0;
    tu_uint8 buf[kBufMaxSize];

    while (index < size) {
        auto remaining = size - index;
        int count = remaining > kBufSegSize? kBufSegSize : remaining;
        memcpy(buf + 2, base + index, count);
        index += count;

        noise_buffer_set_inout(buffer, buf + 2, count, kBufMaxSize - 2);
        ret = noise_cipherstate_encrypt(m_send, &buffer);
        if (ret != NOISE_ERROR_NONE)
            goto err;

        // write the ciphertext size
        tempo_utils::write_u16(buffer.size, buf);

        auto *outputBuf = ArrayBuf::allocate(buf, buffer.size + 2);
        output.push(outputBuf);
    }

    free_stream_buf(streamBuf);     // we free the streamBuf once we are sure encrypt will not fail

    while (!output.empty()) {
        auto *outputBuf = output.front();
        output.pop();
        m_output.push(outputBuf);
    }

    return {};

err:
    while (!output.empty()) {
        free_stream_buf(output.front());
        output.pop();
    }
    return noise_error_to_status(ret);
}

bool
chord_mesh::Cipher::hasOutput() const
{
    return !m_output.empty();
}

chord_mesh::StreamBuf *
chord_mesh::Cipher::popOutput()
{
    if (m_output.empty())
        return nullptr;
    auto *output = m_output.front();
    m_output.pop();
    return output;
}

struct GenerateStaticKeyState {
    NoiseDHState *dh = nullptr;

    ~GenerateStaticKeyState() {
        if (dh != nullptr) {
            noise_dhstate_free(dh);
        }
    }
};

tempo_utils::Status
chord_mesh::generate_static_key(
    std::shared_ptr<tempo_security::PrivateKey> privateKey,
    StaticKeypair &staticKeypair)
{
    GenerateStaticKeyState state;
    int ret;

    ret = noise_dhstate_new_by_id(&state.dh, NOISE_DH_CURVE25519);
    if (ret != NOISE_ERROR_NONE)
        return noise_error_to_status(ret);

    // generate the keypair
    ret = noise_dhstate_generate_keypair(state.dh);
    if (ret != NOISE_ERROR_NONE)
        return noise_error_to_status(ret);

    std::vector<tu_uint8> prvkey(noise_dhstate_get_private_key_length(state.dh));
    std::vector<tu_uint8> pubkey(noise_dhstate_get_public_key_length(state.dh));

    // read the keypair into memory
    ret = noise_dhstate_get_keypair(state.dh,
        prvkey.data(), prvkey.size(),
        pubkey.data(), pubkey.size());
    if (ret != NOISE_ERROR_NONE)
        return noise_error_to_status(ret);

    // generate a signature for the public key
    tempo_security::Digest digest;
    TU_ASSIGN_OR_RETURN (digest, tempo_security::DigestUtils::generate_signed_message_digest(
        pubkey, privateKey, tempo_security::DigestId::None));

    staticKeypair.publicKey = std::move(pubkey);
    staticKeypair.privateKey = std::move(prvkey);
    staticKeypair.digest = digest;

    return {};
}

tempo_utils::Status
chord_mesh::validate_static_key(
    std::span<const tu_uint8> publicKey,
    std::shared_ptr<tempo_security::X509Certificate> certificate,
    std::shared_ptr<tempo_security::X509Store> store,
    const tempo_security::Digest &digest)
{
    if (!store->verifyCertificate(certificate))
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "certificate {} is not valid", certificate->getCommonName());

    bool verified;
    TU_ASSIGN_OR_RETURN (verified, tempo_security::DigestUtils::verify_signed_message_digest(
        publicKey, digest, certificate));
    if (!verified)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "public key signature is not valid");

    return {};
}