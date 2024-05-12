#ifndef CHORD_SANDBOX_REMOTING_CLIENT_H
#define CHORD_SANDBOX_REMOTING_CLIENT_H

#include <chord_protocol/abstract_protocol_handler.h>
#include <chord_protocol/abstract_protocol_writer.h>
#include <chord_remoting/remoting_service.grpc.pb.h>
#include <tempo_utils/url.h>

#include "sandbox_result.h"

namespace chord_sandbox {

    class ClientCommunicationStream
        : public grpc::ClientBidiReactor<
            chord_remoting::Message,
            chord_remoting::Message>,
          public chord_protocol::AbstractProtocolWriter
    {
    public:
        ClientCommunicationStream(
            chord_remoting::RemotingService::StubInterface *stub,
            const tempo_utils::Url &protocolUrl,
            std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler,
            bool freeWhenDone);
        ~ClientCommunicationStream() override;

        void OnReadInitialMetadataDone(bool ok) override;
        void OnReadDone(bool ok) override;
        void OnWriteDone(bool ok) override;
        void OnDone(const grpc::Status &status) override;
        tempo_utils::Status write(std::string_view message) override;

        struct PendingWrite {
            chord_remoting::Message message;
            PendingWrite *next;
        };

    private:
        tempo_utils::Url m_protocolUrl;
        std::shared_ptr<chord_protocol::AbstractProtocolHandler> m_handler;
        bool m_freeWhenDone;
        grpc::ClientContext m_context;
        chord_remoting::Message m_incoming;

        absl::Mutex m_lock;
        PendingWrite *m_head ABSL_GUARDED_BY(m_lock);
        PendingWrite *m_tail ABSL_GUARDED_BY(m_lock);
    };

    class RemotingClient {

    public:
        RemotingClient(
            const tempo_utils::Url &endpointUrl,
            const tempo_utils::Url &protocolUrl,
            std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler,
            std::shared_ptr<grpc::ChannelCredentials> credentials,
            const std::string &endpointServerName);

        tempo_utils::Status connect();
        tempo_utils::Status shutdown();

    private:
        tempo_utils::Url m_endpointUrl;
        tempo_utils::Url m_protocolUrl;
        std::shared_ptr<chord_protocol::AbstractProtocolHandler> m_handler;
        std::shared_ptr<grpc::ChannelCredentials> m_credentials;
        std::string m_endpointServerName;
        std::shared_ptr<grpc::Channel> m_channel;

        absl::Mutex m_lock;
        std::unique_ptr<chord_remoting::RemotingService::StubInterface> m_stub ABSL_GUARDED_BY(m_lock);
    };
}

#endif // CHORD_SANDBOX_REMOTING_CLIENT_H