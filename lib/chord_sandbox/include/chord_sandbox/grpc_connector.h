#ifndef CHORD_SANDBOX_GRPC_CONNECTOR_H
#define CHORD_SANDBOX_GRPC_CONNECTOR_H

#include <filesystem>

#include <google/protobuf/message.h>
#include <grpcpp/security/credentials.h>

#include <chord_common/abstract_protocol_handler.h>
#include <chord_sandbox/sandbox_result.h>
#include <lyric_common/runtime_policy.h>
#include <tempo_utils/url.h>
#include "chord_remoting/remoting_service.grpc.pb.h"

namespace chord_sandbox {

    // forward declarations
    struct ClientPriv;
    class ClientMonitorStream;
    class MachineMonitor;

    /**
     *
     */
    class GrpcConnector {

    public:
        GrpcConnector(
            const tempo_utils::Url &machineUrl,
            const lyric_common::RuntimePolicy &policy);

        std::shared_ptr<MachineMonitor> getMonitor() const;

        tempo_utils::Status registerProtocolHandler(
            const tempo_utils::Url &protocolUrl,
            std::shared_ptr<chord_common::AbstractProtocolHandler> handler,
            const tempo_utils::Url &endpointUrl,
            const std::filesystem::path &pemRootCABundleFile,
            const std::string &endpointServerName);

        tempo_utils::Status connect(
            const tempo_utils::Url &controlUrl,
            const std::filesystem::path &pemRootCABundleFile,
            const std::string &endpointServerName);

        tempo_utils::Status suspend();
        tempo_utils::Status resume();
        tempo_utils::Status terminate();

    private:
        tempo_utils::Url m_machineUrl;
        lyric_common::RuntimePolicy m_policy;
        std::unique_ptr<chord_remoting::RemotingService::StubInterface> m_stub;
        std::shared_ptr<MachineMonitor> m_machineMonitor;

        absl::Mutex m_lock;
        bool m_connected ABSL_GUARDED_BY(m_lock);
        ClientMonitorStream *m_monitorStream ABSL_GUARDED_BY(m_lock);
        absl::flat_hash_map<tempo_utils::Url,std::shared_ptr<ClientPriv>> m_clients ABSL_GUARDED_BY(m_lock);
    };

    /**
     * The state of the remote machine.
     */
    class MachineMonitor {
    public:
        MachineMonitor();

        chord_remoting::MachineState getState();
        void setState(chord_remoting::MachineState state);
        tempo_utils::StatusCode getStatusCode();
        void setStatusCode(tempo_utils::StatusCode statusCode);

        chord_remoting::MachineState waitForStateChange(chord_remoting::MachineState prevState, int timeoutMillis);

    private:
        absl::Mutex m_lock;
        absl::CondVar m_cond;
        chord_remoting::MachineState m_state ABSL_GUARDED_BY(m_lock);
        tempo_utils::StatusCode m_statusCode ABSL_GUARDED_BY(m_lock);
    };

    /**
     * Stream of monitor events.
     */
    class ClientMonitorStream : public grpc::ClientReadReactor<chord_remoting::MonitorEvent> {
    public:
        ClientMonitorStream(
            chord_remoting::RemotingService::StubInterface *stub,
            std::shared_ptr<MachineMonitor> machineMonitor,
            bool freeWhenDone);
        ~ClientMonitorStream() override;

        void OnReadInitialMetadataDone(bool ok) override;
        void OnReadDone(bool ok) override;
        void OnDone(const grpc::Status &status) override;

    private:
        std::shared_ptr<MachineMonitor> m_machineMonitor;
        bool m_freeWhenDone;
        grpc::ClientContext m_context;
        chord_remoting::MonitorEvent m_incoming;
    };

    /**
     * Validates per-call credentials.
     */
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
