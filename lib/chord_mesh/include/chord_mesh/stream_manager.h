#ifndef CHORD_MESH_STREAM_MANAGER_H
#define CHORD_MESH_STREAM_MANAGER_H

#include <uv.h>

#include <tempo_security/certificate_key_pair.h>
#include <tempo_security/x509_store.h>
#include <tempo_utils/uuid.h>

#include "envelope.h"

namespace chord_mesh {

    class Stream;
    class StreamManager;
    class StreamSession;

    enum class ConnectState {
        Pending,
        Complete,
        Failed,
        Aborted,
    };

    enum class AcceptState {
        Initial,
        Active,
        ShuttingDown,
        Closing,
        Closed,
    };

    enum class StreamState {
        Initial,
        Active,
        ShuttingDown,
        Closing,
        Closed,
    };

    class AbstractConnectContext {
    public:
        virtual ~AbstractConnectContext() = default;
        virtual void connect(std::shared_ptr<Stream>) = 0;
        virtual void error(const tempo_utils::Status &) = 0;
        virtual void cleanup() = 0;
    };

    class AbstractAcceptContext {
    public:
        virtual ~AbstractAcceptContext() = default;
        virtual void accept(std::shared_ptr<Stream>) = 0;
        virtual void error(const tempo_utils::Status &) = 0;
        virtual void cleanup() = 0;
    };

    class AbstractStreamContext {
    public:
        virtual ~AbstractStreamContext() = default;
        virtual void receive(const Envelope &) = 0;
        virtual tempo_utils::Status validate(std::string_view, std::shared_ptr<tempo_security::X509Certificate>) = 0;
        virtual void error(const tempo_utils::Status &) = 0;
        virtual void cleanup() = 0;
    };

    struct ConnectHandle {
        uv_connect_t *req;
        StreamManager *manager;
        bool insecure;
        std::unique_ptr<AbstractConnectContext> ctx;
        tempo_utils::UUID id;
        ConnectState state;
        ConnectHandle *prev;
        ConnectHandle *next;

        ConnectHandle(
            uv_connect_t *req,
            StreamManager *manager,
            bool insecure,
            std::unique_ptr<AbstractConnectContext> &&ctx);
        ~ConnectHandle();

        void connect(std::shared_ptr<Stream> stream);
        void error(const tempo_utils::Status &status);
        void abort();
        void release();

    private:
        bool m_shared;

        friend void close_connect(uv_handle_t *stream);
    };

    struct AcceptHandle {
        uv_stream_t *stream;
        StreamManager *manager;
        bool insecure;
        std::unique_ptr<AbstractAcceptContext> ctx;
        tempo_utils::UUID id;
        AcceptState state;
        AcceptHandle *prev;
        AcceptHandle *next;

        AcceptHandle(
            uv_stream_t *stream,
            StreamManager *manager,
            bool insecure,
            std::unique_ptr<AbstractAcceptContext> &&ctx);
        ~AcceptHandle();
        void accept(std::shared_ptr<Stream> stream);
        void error(const tempo_utils::Status &status);
        void shutdown();
        void close();
        void release();

    private:
        bool m_shared;
        uv_shutdown_t m_req;

        friend void close_accept(uv_handle_t *stream);
    };

    struct StreamHandle {
        uv_stream_t *stream;
        StreamManager *manager;
        bool initiator;
        bool insecure;
        std::unique_ptr<StreamSession> session;
        std::unique_ptr<AbstractStreamContext> ctx;
        tempo_utils::UUID id;
        StreamState state;
        StreamHandle *prev;
        StreamHandle *next;

        StreamHandle(uv_stream_t *stream, StreamManager *manager, bool initiator, bool insecure);
        ~StreamHandle();
        tempo_utils::Status start(std::unique_ptr<AbstractStreamContext> &&ctx);
        tempo_utils::Status negotiate(std::string_view protocolName);
        tempo_utils::Status validate(std::string_view protocolName, std::shared_ptr<tempo_security::X509Certificate> certificate);
        tempo_utils::Status send(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes);
        void receive(const Envelope &envelope);
        void error(const tempo_utils::Status &status);
        void shutdown();
        void close();
        void release();

    private:
        bool m_shared;
        uv_shutdown_t m_req;

        friend void close_stream(uv_handle_t *stream);
    };

    struct StreamManagerOps {
        void (*error)(const tempo_utils::Status &, void *) = nullptr;
        void (*cleanup)(void *) = nullptr;
    };

    struct StreamManagerOptions {
        std::string protocolName = {};
        void *data = nullptr;
    };

    class StreamManager {
    public:
        StreamManager(
            uv_loop_t *loop,
            const tempo_security::CertificateKeyPair &keypair,
            std::shared_ptr<tempo_security::X509Store> trustStore,
            const StreamManagerOps &ops,
            const StreamManagerOptions &options = {});

        uv_loop_t *getLoop() const;
        std::shared_ptr<tempo_security::X509Store> getTrustStore() const;
        tempo_security::CertificateKeyPair getKeypair() const;

        std::string getProtocolName() const;

        ConnectHandle *allocateConnectHandle(
            uv_connect_t *connect,
            bool insecure,
            std::unique_ptr<AbstractConnectContext> &&ctx);
        AcceptHandle *allocateAcceptHandle(
            uv_stream_t *accept,
            bool insecure,
            std::unique_ptr<AbstractAcceptContext> &&ctx);
        StreamHandle *allocateStreamHandle(
            uv_stream_t *stream,
            bool initiator,
            bool insecure);
        void shutdown();

    private:
        uv_loop_t *m_loop;
        tempo_security::CertificateKeyPair m_keypair;
        std::shared_ptr<tempo_security::X509Store> m_trustStore;
        StreamManagerOps m_ops;
        StreamManagerOptions m_options;

        ConnectHandle *m_connects;
        AcceptHandle *m_accepts;
        StreamHandle *m_streams;
        bool m_running;

        void freeConnectHandle(ConnectHandle *handle);
        void freeAcceptHandle(AcceptHandle *handle);
        void freeStreamHandle(StreamHandle *handle);
        void emitError(const tempo_utils::Status &status);

        friend struct ConnectHandle;
        friend struct AcceptHandle;
        friend struct StreamHandle;
        friend void close_connect(uv_handle_t *connect);
        friend void shutdown_accept(uv_shutdown_t *req, int err);
        friend void close_accept(uv_handle_t *stream);
        friend void shutdown_stream(uv_shutdown_t *req, int err);
        friend void close_stream(uv_handle_t *stream);
    };
}

#endif // CHORD_MESH_STREAM_MANAGER_H