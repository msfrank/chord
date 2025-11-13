#ifndef CHORD_MESH_NOISE_H
#define CHORD_MESH_NOISE_H

#include <noise/protocol/handshakestate.h>
#include <noise/protocol/names.h>

#include <tempo_security/digest_utils.h>
#include <tempo_security/private_key.h>
#include <tempo_security/x509_certificate.h>
#include <tempo_security/x509_store.h>
#include <tempo_utils/bytes_appender.h>

#include "message.h"
#include "stream_buf.h"

namespace chord_mesh {

    constexpr const char *kDefaultNoiseProtocol = "Noise_KK_25519_ChaChaPoly_BLAKE2s";

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

    enum class HandshakeState {
        Initial,
        Waiting,
        Split,
        Failed,
    };

    class Cipher;

    class Handshake {
    public:
        virtual ~Handshake();

        tempo_utils::Status start();
        tempo_utils::Status process(const tu_uint8 *data, size_t size);
        tempo_utils::Result<std::shared_ptr<Cipher>> finish();

        HandshakeState getHandshakeState() const;

        bool hasOutgoing() const;
        std::shared_ptr<const tempo_utils::ImmutableBytes> popOutgoing();

        static tempo_utils::Result<std::shared_ptr<Handshake>> forInitiator(
            std::string_view protocolName,
            std::span<const tu_uint8> localPrivateKey,
            std::span<const tu_uint8> remotePublicKey,
            std::span<const tu_uint8> prologue = {});

        static tempo_utils::Result<std::shared_ptr<Handshake>> forResponder(
            std::string_view protocolName,
            std::span<const tu_uint8> localPrivateKey,
            std::span<const tu_uint8> remotePublicKey,
            std::span<const tu_uint8> prologue = {});

        static tempo_utils::Result<std::shared_ptr<Handshake>> create(
            std::string_view protocolName,
            bool initiator,
            std::span<const tu_uint8> localPrivateKey,
            std::span<const tu_uint8> remotePublicKey,
            std::span<const tu_uint8> prologue = {});

    private:
        HandshakeState m_state;
        NoiseHandshakeState *m_handshake;
        std::queue<std::shared_ptr<const tempo_utils::ImmutableBytes>> m_outgoing;

        Handshake();
        tempo_utils::Status initialize(
            std::string_view protocolName,
            int role,
            std::span<const tu_uint8> prologue,
            std::span<const tu_uint8> localPrivateKey,
            std::span<const tu_uint8> remotePublicKey);
    };

    class Cipher {
    public:
        virtual ~Cipher();

        tempo_utils::Status decryptInput(const tu_uint8 *data, size_t size);
        bool hasInput() const;
        std::shared_ptr<const tempo_utils::ImmutableBytes> popInput();

        tempo_utils::Status encryptOutput(StreamBuf *streamBuf);
        bool hasOutput() const;
        StreamBuf *popOutput();

    private:
        NoiseCipherState *m_send;
        NoiseCipherState *m_recv;
        std::unique_ptr<tempo_utils::BytesAppender> m_pending;
        std::queue<std::shared_ptr<const tempo_utils::ImmutableBytes>> m_input;
        std::queue<StreamBuf *> m_output;

        Cipher();
        tempo_utils::Status initialize(NoiseHandshakeState *handshake);
        friend class Handshake;
    };
}

#endif // CHORD_MESH_NOISE_H