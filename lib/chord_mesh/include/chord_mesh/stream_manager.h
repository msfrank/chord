#ifndef CHORD_MESH_STREAM_MANAGER_H
#define CHORD_MESH_STREAM_MANAGER_H

#include <uv.h>

#include <tempo_security/certificate_key_pair.h>
#include <tempo_security/x509_store.h>
#include <tempo_utils/result.h>

namespace chord_mesh {

    class StreamManager;

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

    class StreamManager {
    public:
        StreamManager(
            uv_loop_t *loop,
            const tempo_security::CertificateKeyPair &keypair,
            std::shared_ptr<tempo_security::X509Store> trustStore,
            const StreamManagerOps &ops,
            void *data = nullptr);

        uv_loop_t *getLoop() const;
        std::shared_ptr<tempo_security::X509Store> getTrustStore() const;
        tempo_security::CertificateKeyPair getKeypair() const;

        StreamHandle *allocateHandle(uv_stream_t *stream, void *data = nullptr);
        void shutdown();

    private:
        uv_loop_t *m_loop;
        tempo_security::CertificateKeyPair m_keypair;
        std::shared_ptr<tempo_security::X509Store> m_trustStore;
        StreamManagerOps m_ops;
        void *m_data;
        StreamHandle *m_handles;
        bool m_running;

        void freeHandle(StreamHandle *handle);
        void emitError(const tempo_utils::Status &status);

        friend struct StreamHandle;
        friend void shutdown_stream(uv_shutdown_t *req, int err);
        friend void close_stream(uv_handle_t *stream);
    };
}

#endif // CHORD_MESH_STREAM_MANAGER_H