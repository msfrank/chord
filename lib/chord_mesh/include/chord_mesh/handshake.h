#ifndef CHORD_MESH_HANDSHAKE_H
#define CHORD_MESH_HANDSHAKE_H

#include <noise/protocol/handshakestate.h>
#include <noise/protocol/names.h>
#include <tempo_security/digest_utils.h>

#include <tempo_security/private_key.h>
#include <tempo_security/x509_certificate.h>
#include <tempo_security/x509_store.h>

namespace chord_mesh {

    struct StaticKeypair {
        std::vector<tu_uint8> publicKey;
        std::vector<tu_uint8> privateKey;
        tempo_security::Digest digest;
    };

    tempo_utils::Status generate_static_key(
        std::shared_ptr<tempo_security::PrivateKey> privateKey,
        StaticKeypair &staticKeypair);

    tempo_utils::Status validate_static_key(
        std::span<const tu_uint8> publicKey,
        std::shared_ptr<tempo_security::X509Certificate> certificate,
        std::shared_ptr<tempo_security::X509Store> store,
        const tempo_security::Digest &digest);

    class Handshake {
    public:
        virtual ~Handshake();

        static tempo_utils::Result<std::shared_ptr<Handshake>> forInitiator(
            const NoiseProtocolId *protocolId,
            std::span<const tu_uint8> prologue,
            std::span<const tu_uint8> localPrivateKey,
            std::span<const tu_uint8> remotePublicKey);

        static tempo_utils::Result<std::shared_ptr<Handshake>> forResponder(
            const NoiseProtocolId *protocolId,
            std::span<const tu_uint8> prologue,
            std::span<const tu_uint8> localPrivateKey,
            std::span<const tu_uint8> remotePublicKey);

    private:
        NoiseHandshakeState *m_state;

        Handshake();
        tempo_utils::Status initialize(
            const NoiseProtocolId *protocolId,
            int role,
            std::span<const tu_uint8> prologue,
            std::span<const tu_uint8> localPrivateKey,
            std::span<const tu_uint8> remotePublicKey);
    };
}

#endif // CHORD_MESH_HANDSHAKE_H