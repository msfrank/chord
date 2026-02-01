#ifndef CHORD_MESH_STREAM_MANAGER_H
#define CHORD_MESH_STREAM_MANAGER_H

#include <uv.h>

#include <tempo_security/certificate_key_pair.h>
#include <tempo_security/x509_store.h>
#include <tempo_utils/result.h>
#include <tempo_utils/uuid.h>

namespace chord_mesh {

    class StreamManager;
    class Stream;

    enum class ConnectState {
        Pending,
        Complete,
        Failed,
        Aborted,
    };

    class AbstractConnectContext {
    public:
        virtual ~AbstractConnectContext() = default;
        virtual void connect(std::shared_ptr<Stream> stream) = 0;
        virtual void error(const tempo_utils::Status &status) = 0;
        virtual void cleanup() = 0;
    };

    struct ConnectHandle {
        uv_connect_t *req;
        StreamManager *manager;
        bool insecure;
        std::unique_ptr<AbstractConnectContext> ctx;
        tempo_utils::UUID id;
        ConnectState state;
        bool shared;
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
    };

    struct StreamHandle {
        uv_stream_t *stream;
        StreamManager *manager;
        void *data;
        StreamHandle *prev;
        StreamHandle *next;

        StreamHandle(uv_stream_t *stream, StreamManager *manager, void *data);
        void shutdown();
        void close();

    private:
        uv_shutdown_t m_req;
        bool m_closing;
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
        StreamHandle *allocateStreamHandle(uv_stream_t *stream, void *data = nullptr);
        void shutdown();

    private:
        uv_loop_t *m_loop;
        tempo_security::CertificateKeyPair m_keypair;
        std::shared_ptr<tempo_security::X509Store> m_trustStore;
        StreamManagerOps m_ops;
        StreamManagerOptions m_options;

        ConnectHandle *m_connects;
        StreamHandle *m_streams;
        bool m_running;

        void freeConnectHandle(ConnectHandle *handle);
        void freeStreamHandle(StreamHandle *handle);
        void emitError(const tempo_utils::Status &status);

        friend struct ConnectHandle;
        friend struct StreamHandle;
        friend void close_connect(uv_handle_t *connect);
        friend void shutdown_stream(uv_shutdown_t *req, int err);
        friend void close_stream(uv_handle_t *stream);
    };
}

#endif // CHORD_MESH_STREAM_MANAGER_H