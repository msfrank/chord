
#include <grpcpp/security/server_credentials.h>

#include <chord_local_machine/grpc_binder.h>
#include <tempo_utils/file_reader.h>
#include <tempo_utils/log_stream.h>

chord_machine::GrpcBinder::GrpcBinder(
        std::string_view endpoint,
        const lyric_common::RuntimePolicy &policy,
        const std::filesystem::path &pemPrivateKeyFile,
        const std::filesystem::path &pemRootCABundleFile,
        chord_remoting::RemotingService::CallbackService *remotingService)
    : m_endpoint(endpoint),
      m_policy(policy),
      m_pemPrivateKeyFile(pemPrivateKeyFile),
      m_pemRootCABundleFile(pemRootCABundleFile),
      m_remotingService(remotingService)
{
    TU_ASSERT (!m_endpoint.empty());
    TU_ASSERT (!m_pemPrivateKeyFile.empty());
    TU_ASSERT (!m_pemRootCABundleFile.empty());
    TU_ASSERT (m_remotingService != nullptr);
}

tempo_utils::Status
chord_machine::GrpcBinder::initialize(const std::filesystem::path &pemCertificateFile)
{
    TU_LOG_FATAL_IF (m_server != nullptr) << "transport is already running";

    tempo_utils::FileReader certificateReader(pemCertificateFile);
    if (!certificateReader.isValid())
        return certificateReader.getStatus();
    tempo_utils::FileReader privateKeyReader(m_pemPrivateKeyFile);
    if (!privateKeyReader.isValid())
        return privateKeyReader.getStatus();
    tempo_utils::FileReader rootCABundleReader(m_pemRootCABundleFile);
    if (!rootCABundleReader.isValid())
        return rootCABundleReader.getStatus();

    grpc::SslServerCredentialsOptions options;
    grpc::SslServerCredentialsOptions::PemKeyCertPair pair;
    auto rootCABytes = rootCABundleReader.getBytes();
    auto certificateBytes = certificateReader.getBytes();
    auto privateKeyBytes = privateKeyReader.getBytes();
    options.pem_root_certs = std::string((const char *) rootCABytes->getData(), rootCABytes->getSize());
    pair.cert_chain = std::string((const char *) certificateBytes->getData(), certificateBytes->getSize());
    pair.private_key = std::string((const char *) privateKeyBytes->getData(), privateKeyBytes->getSize());
    options.pem_key_cert_pairs.push_back(pair);

    m_credentials = grpc::SslServerCredentials(options);
    auto processor = std::make_shared<DriverMetadataProcessor>(this);
    m_credentials->SetAuthMetadataProcessor(processor);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(m_endpoint, m_credentials);
    builder.RegisterService(m_remotingService);

    TU_LOG_VV << "starting grpc transport";

    m_server = builder.BuildAndStart();

    return {};
}

tempo_utils::Status
chord_machine::GrpcBinder::shutdown()
{
    if (m_server == nullptr)
        return {};

    TU_LOG_VV << "shutting down grpc transport";
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds{5};
    m_server->Shutdown(deadline);
    return {};
}

chord_machine::DriverMetadataProcessor::DriverMetadataProcessor(GrpcBinder *binder)
    : grpc::AuthMetadataProcessor(),
      m_binder(binder)
{
    TU_ASSERT (m_binder != nullptr);
}

grpc::Status
chord_machine::DriverMetadataProcessor::Process(
    const InputMetadata &auth_metadata,
    grpc::AuthContext *context,
    OutputMetadata *consumed_auth_metadata,
    OutputMetadata *response_metadata)
{
//    auto kv = auth_metadata.find("zuri-principal");
//    if (kv == auth_metadata.end())
//        return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "missing zuri-principal");
//    auto principal = std::string(kv->second.data());
//
//    // after verification, mark metadata entry as consumed
//    consumed_auth_metadata->insert(std::make_pair("zuri-principal", principal));
//
//    // add property to the context and make it available via GetPeerIdentity
//    context->AddProperty("zuri-principal", principal);
//    context->SetPeerIdentityPropertyName("zuri-principal");

    return grpc::Status::OK;
}
