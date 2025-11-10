
#include <chord_mesh/handshake.h>
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/supervisor_node.h>

chord_mesh::Handshake::Handshake()
    : m_state(nullptr)
{
}

chord_mesh::Handshake::~Handshake()
{
    if (m_state != nullptr) {
        noise_handshakestate_free(m_state);
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
    TU_ASSERT (m_state == nullptr);
    auto ret = noise_handshakestate_new_by_id(&m_state, protocolId, role);
    if (ret != NOISE_ERROR_NONE)
        return noise_error_to_status(ret);

    // if a prologue was specified then set it
    if (!prologue.empty()) {
        ret = noise_handshakestate_set_prologue(m_state, prologue.data(), prologue.size());
        if (ret != NOISE_ERROR_NONE)
            return noise_error_to_status(ret);
    }

    // set the local keypair if required
    if (noise_handshakestate_needs_local_keypair(m_state)) {
        if (localPrivateKey.empty())
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "protocol requires local private key");
        auto *dh = noise_handshakestate_get_local_keypair_dh(m_state);
        ret = noise_dhstate_set_keypair_private(dh, localPrivateKey.data(), localPrivateKey.size());
        if (ret != NOISE_ERROR_NONE)
            return noise_error_to_status(ret);
    }

    if (noise_handshakestate_needs_remote_public_key(m_state)) {
        if (remotePublicKey.empty())
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "protocol requires remote public key");
        auto *dh = noise_handshakestate_get_remote_public_key_dh(m_state);
        ret = noise_dhstate_set_public_key(dh, remotePublicKey.data(), remotePublicKey.size());
        if (ret != NOISE_ERROR_NONE)
            return noise_error_to_status(ret);
    }

    return {};
}

tempo_utils::Result<std::shared_ptr<chord_mesh::Handshake>>
chord_mesh::Handshake::forInitiator(
    const NoiseProtocolId *protocolId,
    std::span<const tu_uint8> prologue,
    std::span<const tu_uint8> localPrivateKey,
    std::span<const tu_uint8> remotePublicKey)
{
    auto initiator = std::shared_ptr<Handshake>(new Handshake());
    TU_RETURN_IF_NOT_OK (initiator->initialize(
        protocolId, NOISE_ROLE_INITIATOR, prologue, localPrivateKey, remotePublicKey));
    return initiator;
}

tempo_utils::Result<std::shared_ptr<chord_mesh::Handshake>>
chord_mesh::Handshake::forResponder(
    const NoiseProtocolId *protocolId,
    std::span<const tu_uint8> prologue,
    std::span<const tu_uint8> localPrivateKey,
    std::span<const tu_uint8> remotePublicKey)
{
    auto initiator = std::shared_ptr<Handshake>(new Handshake());
    TU_RETURN_IF_NOT_OK (initiator->initialize(
        protocolId, NOISE_ROLE_RESPONDER, prologue, localPrivateKey, remotePublicKey));
    return initiator;
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