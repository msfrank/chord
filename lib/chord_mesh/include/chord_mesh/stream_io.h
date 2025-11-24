#ifndef CHORD_MESH_STREAM_IO_H
#define CHORD_MESH_STREAM_IO_H

#include <tempo_utils/immutable_bytes.h>
#include <tempo_utils/status.h>

#include "noise.h"
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

    class Pending {
    public:
        Pending();
        ~Pending();

        void pushIncoming(std::span<const tu_uint8> incoming);
        bool hasIncoming() const;
        std::shared_ptr<const tempo_utils::ImmutableBytes> popIncoming();
        void pushOutgoing(StreamBuf *streamBuf);
        bool hasOutgoing() const;
        StreamBuf *popOutgoing();

    private:
        std::unique_ptr<tempo_utils::BytesAppender> m_incoming;
        std::queue<StreamBuf *> m_outgoing;
    };

    class InitialStreamBehavior : public AbstractStreamBehavior {
    public:
        InitialStreamBehavior();
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        std::unique_ptr<Pending>&& takePending();

    private:
        std::unique_ptr<Pending> m_pending;
    };

    class InsecureStreamBehavior : public AbstractStreamBehavior {
    public:
        explicit InsecureStreamBehavior(bool secure);
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        tempo_utils::Status negotiate(
            std::shared_ptr<const tempo_utils::ImmutableBytes> bytes,
            AbstractStreamBufWriter *writer);
        tempo_utils::Status applyPending(std::unique_ptr<Pending> &&pending, AbstractStreamBufWriter *writer);
        std::unique_ptr<Pending>&& takePending();

    private:
        bool m_secure;
        MessageParser m_parser;
        std::unique_ptr<Pending> m_pending;
    };

    class PendingLocalStreamBehavior : public AbstractStreamBehavior {
    public:
        PendingLocalStreamBehavior(
            std::string_view protocolName,
            std::span<const tu_uint8> remotePublicKey,
            std::unique_ptr<Pending> &&pending);
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        std::string getRemoteProtocolName() const;
        std::span<const tu_uint8> getRemotePublicKey() const;
        tempo_utils::Status negotiate(
            std::shared_ptr<const tempo_utils::ImmutableBytes> bytes,
            AbstractStreamBufWriter *writer);

        std::unique_ptr<Pending>&& takePending();

    private:
        std::string m_protocolName;
        std::vector<tu_uint8> m_remotePublicKey;
        std::unique_ptr<Pending> m_pending;
    };

    class PendingRemoteStreamBehavior : public AbstractStreamBehavior {
    public:
        PendingRemoteStreamBehavior(
            std::string_view protocolName,
            std::span<const tu_uint8> localPrivateKey,
            std::unique_ptr<Pending> &&pending);
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        std::string getLocalProtocolName() const;
        std::span<const tu_uint8> getLocalPrivateKey() const;

        std::unique_ptr<Pending>&& takePending();

    private:
        std::string m_protocolName;
        std::vector<tu_uint8> m_localPrivateKey;
        std::unique_ptr<Pending> m_pending;
        MessageParser m_parser;
    };

    class HandshakingStreamBehavior : public AbstractStreamBehavior {
    public:
        HandshakingStreamBehavior(std::shared_ptr<Handshake> handshake, std::unique_ptr<Pending> &&pending);
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        tempo_utils::Status start(AbstractStreamBufWriter *writer);
        tempo_utils::Status process(std::span<const tu_uint8> data, AbstractStreamBufWriter *writer, bool &finished);
        tempo_utils::Result<std::shared_ptr<Cipher>> finish();

        std::unique_ptr<Pending>&& takePending();

    private:
        std::shared_ptr<Handshake> m_handshake;
        std::unique_ptr<Pending> m_pending;
        MessageParser m_parser;
    };

    class SecureStreamBehavior : public AbstractStreamBehavior {
    public:
        SecureStreamBehavior(std::shared_ptr<Cipher> cipher, std::unique_ptr<Pending> &&pending);
        tempo_utils::Status read(const tu_uint8 *data, ssize_t size) override;
        tempo_utils::Status write(AbstractStreamBufWriter *writer, StreamBuf *streamBuf) override;
        tempo_utils::Status check(bool &ready) override;
        tempo_utils::Status take(Message &message) override;

        tempo_utils::Status start(AbstractStreamBufWriter *writer);
        std::shared_ptr<const tempo_utils::ImmutableBytes> takePending();

    private:
        std::shared_ptr<Cipher> m_cipher;
        std::unique_ptr<Pending> m_pending;
        MessageParser m_parser;
    };

    class StreamIO {
    public:
        StreamIO(bool initiator, StreamManager *manager, AbstractStreamBufWriter *writer);

        IOState getIOState() const;

        tempo_utils::Status start(bool secure);
        tempo_utils::Status negotiateRemote(
            std::string_view protocolName,
            std::shared_ptr<tempo_security::X509Certificate> certificate,
            std::span<const tu_uint8> remotePublicKey,
            const tempo_security::Digest &digest);
        tempo_utils::Status negotiateLocal(
            std::string_view protocolName,
            std::shared_ptr<tempo_security::X509Certificate> certificate,
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