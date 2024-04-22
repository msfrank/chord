#ifndef CHORD_SANDBOX_GRPC_CONNECTOR_H
#define CHORD_SANDBOX_GRPC_CONNECTOR_H

#include <filesystem>

#include <google/protobuf/message.h>
#include <grpcpp/security/credentials.h>

#include <chord_protocol/abstract_protocol_handler.h>
#include <chord_sandbox/sandbox_result.h>
#include <lyric_common/runtime_policy.h>
#include <tempo_utils/url.h>

namespace chord_sandbox {

    struct ClientPriv;

    class GrpcConnector {

    public:
        GrpcConnector(const tempo_utils::Url &machineUrl, const lyric_common::RuntimePolicy &policy);

        SandboxStatus registerProtocolHandler(
            const tempo_utils::Url &protocolUrl,
            std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler,
            const tempo_utils::Url &endpointUrl,
            const std::filesystem::path &pemRootCABundleFile,
            const std::string &endpointServerName);

        tempo_utils::Status start();
        void stop();

    private:
        tempo_utils::Url m_machineUrl;
        lyric_common::RuntimePolicy m_policy;

        absl::Mutex m_lock;
        absl::flat_hash_map<tempo_utils::Url,std::shared_ptr<ClientPriv>> m_clients;
        bool m_running;
    };

    class JwtCallCredentialsPlugin : public grpc::MetadataCredentialsPlugin {

    public:
        JwtCallCredentialsPlugin(const std::string &pemCertificate, const google::protobuf::Message *message);

        const char* GetType() const override;
        grpc::Status GetMetadata(
            grpc::string_ref serviceUrl,
            grpc::string_ref methodName,
            const grpc::AuthContext &channelAuthContext,
            std::multimap<grpc::string, grpc::string> *metadata) override;
        bool IsBlocking() const override;
        grpc::string DebugString() override;

    private:
        std::string m_pemCertificate;
        //const google::protobuf::Message *m_message;
    };
}

#endif // CHORD_SANDBOX_GRPC_CONNECTOR_H
