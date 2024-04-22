#ifndef CHORD_LOCAL_MACHINE_GRPC_BINDER_H
#define CHORD_LOCAL_MACHINE_GRPC_BINDER_H

#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>
#include <grpcpp/grpcpp.h>
#include <uv.h>

#include <chord_protocol/abstract_protocol_handler.h>
#include <lyric_common/runtime_policy.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_utils/url.h>

#include "remoting_service.h"

class GrpcBinder {

public:
    GrpcBinder(
        std::string_view endpoint,
        const lyric_common::RuntimePolicy &policy,
        const std::filesystem::path &pemPrivateKeyFile,
        const std::filesystem::path &pemRootCABundleFile,
        chord_remoting::RemotingService::CallbackService *remotingService);
    virtual ~GrpcBinder() = default;

    tempo_utils::Status initialize(const std::filesystem::path &pemCertificateFile);
    tempo_utils::Status shutdown();

private:
    std::string m_endpoint;
    lyric_common::RuntimePolicy m_policy;
    std::filesystem::path m_pemPrivateKeyFile;
    std::filesystem::path m_pemRootCABundleFile;
    std::shared_ptr<grpc::ServerCredentials> m_credentials;
    chord_remoting::RemotingService::CallbackService *m_remotingService;
    std::unique_ptr<grpc::Server> m_server;
};

class DriverMetadataProcessor : public grpc::AuthMetadataProcessor {
public:
    DriverMetadataProcessor(GrpcBinder *binder);

    grpc::Status Process(
        const InputMetadata &auth_metadata,
        grpc::AuthContext *context,
        OutputMetadata *consumed_auth_metadata,
        OutputMetadata *response_metadata) override;

private:
    GrpcBinder *m_binder;
};

#endif // CHORD_LOCAL_MACHINE_GRPC_BINDER_H