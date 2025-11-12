#ifndef CHORD_MESH_STREAM_IO_H
#define CHORD_MESH_STREAM_IO_H

#include <tempo_utils/immutable_bytes.h>
#include <tempo_utils/status.h>

#include "handshake.h"
#include "message.h"
#include "stream_buf.h"
#include "stream_manager.h"

namespace chord_mesh {

    enum class IOState {
        Initial,
        Insecure,
        PendingLocal,
        PendingRemote,
        Handshaking,
        Secure,
    };

    class AbstractStreamBehavior {
    public:
        virtual ~AbstractStreamBehavior() = default;
        virtual tempo_utils::Status read(const tu_uint8 *data, ssize_t size) = 0;
        virtual tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) = 0;
        virtual tempo_utils::Status check(bool &ready) = 0;
        virtual tempo_utils::Status take(Message &message) = 0;
    };

    class InitialStreamBehavior : public AbstractStreamBehavior {
    public:
        InitialStreamBehavior() = default;
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        bool hasOutgoing() const;
        StreamBuf *takeOutgoing();

    private:
        std::queue<StreamBuf *> m_outgoing;
    };

    class InsecureStreamBehavior : public AbstractStreamBehavior {
    public:
        InsecureStreamBehavior() = default;
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        std::shared_ptr<const tempo_utils::ImmutableBytes> takePending();

    private:
        MessageParser m_parser;
    };

    class PendingLocalStreamBehavior : public AbstractStreamBehavior {
    public:
        PendingLocalStreamBehavior(
            std::span<const tu_uint8> pending,
            std::span<const tu_uint8> remotePublicKey);
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        std::span<const tu_uint8> getRemotePublicKey() const;

    private:
        tempo_utils::BytesAppender m_pending;
        std::vector<tu_uint8> m_remotePublicKey;
    };

    class PendingRemoteStreamBehavior : public AbstractStreamBehavior {
    public:
        PendingRemoteStreamBehavior(
            std::span<const tu_uint8> pending,
            std::span<const tu_uint8> localPrivateKey);
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        std::span<const tu_uint8> getLocalPrivateKey() const;

    private:
        tempo_utils::BytesAppender m_pending;
        std::vector<tu_uint8> m_localPrivateKey;
    };

    class HandshakingStreamBehavior : public AbstractStreamBehavior {
    public:
        explicit HandshakingStreamBehavior(
            std::shared_ptr<Handshake> handshake,
            AbstractStreamBufWriter *writer);
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        tempo_utils::Status start();
        tempo_utils::Status process(std::span<const tu_uint8> data, AbstractStreamBufWriter *writer, bool &finished);
        tempo_utils::Result<std::shared_ptr<Cipher>> finish();
        std::shared_ptr<const tempo_utils::ImmutableBytes> takePending();

    private:
        std::shared_ptr<Handshake> m_handshake;
        AbstractStreamBufWriter *m_writer;
        MessageParser m_parser;
    };

    class SecureStreamBehavior : public AbstractStreamBehavior {
    public:
        explicit SecureStreamBehavior(std::shared_ptr<Cipher> cipher);
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        tempo_utils::Status start(std::span<const tu_uint8> pending);
        std::shared_ptr<const tempo_utils::ImmutableBytes> takePending();

    private:
        std::shared_ptr<Cipher> m_cipher;
        MessageParser m_parser;
    };

    class StreamIO {
    public:
        StreamIO(bool initiator, StreamManager *manager, AbstractStreamBufWriter *writer);

        IOState getIOState() const;

        tempo_utils::Status start();
        tempo_utils::Status negotiateRemote(
            std::span<const tu_uint8> remotePublicKey,
            std::string_view pemCertificateString,
            const tempo_security::Digest &digest);
        tempo_utils::Status negotiateLocal(
            std::string_view pemCertificateString,
            const StaticKeypair &localKeypair);
        tempo_utils::Status processHandshake(std::span<const tu_uint8> data, bool &finished);

        tempo_utils::Status read(const tu_uint8 *data, ssize_t size);
        tempo_utils::Status write(StreamBuf *streamBuf);
        tempo_utils::Status write(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes);

        tempo_utils::Status checkReady(bool &ready);
        tempo_utils::Status takeReady(Message &message);

    private:
        bool m_initiator;
        StreamManager *m_manager;
        AbstractStreamBufWriter *m_writer;
        IOState m_state;
        std::unique_ptr<AbstractStreamBehavior> m_behavior;
    };
}

#endif // CHORD_MESH_STREAM_IO_H