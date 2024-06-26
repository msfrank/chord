
#include <chord_local_machine/initialize_utils.h>
#include <chord_local_machine/interpreter_runner.h>
#include <chord_invoke/invoke_service.grpc.pb.h>
#include <tempo_command/command_result.h>
#include <tempo_security/ecc_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_security/x509_certificate_signing_request.h>
#include <tempo_utils/file_reader.h>
#include <tempo_utils/tempfile_maker.h>
#include <lyric_packaging/directory_loader.h>
#include <lyric_packaging/package_loader.h>
#include <lyric_runtime/chain_loader.h>
#include <lyric_bootstrap/bootstrap_loader.h>

tempo_utils::Status
make_interpreter_state(
    std::shared_ptr<lyric_runtime::InterpreterState> &interpreterState,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig)
{
    lyric_runtime::InterpreterStateOptions interpreterOptions;

    // create the list of loaders with the bootstrap loader in front
    std::vector<std::shared_ptr<lyric_runtime::AbstractLoader>> loaderChain;
    loaderChain.push_back(std::make_shared<lyric_bootstrap::BootstrapLoader>());

    // if install directory was specified then append directory loader
    if (!chordLocalMachineConfig.installDirectory.empty()) {
        loaderChain.push_back(
            std::make_shared<lyric_packaging::DirectoryLoader>(chordLocalMachineConfig.installDirectory));
    }

    // if package directories are specified then append package loader
    if (!chordLocalMachineConfig.packageDirectories.empty()) {
        loaderChain.push_back(
            std::make_shared<lyric_packaging::PackageLoader>(chordLocalMachineConfig.packageDirectories));
    }

    // construct the loader chain
    interpreterOptions.loader = std::make_shared<lyric_runtime::ChainLoader>(loaderChain);

    // construct the interpreter state
    interpreterState = componentConstructor.createInterpreterState(
        interpreterOptions, chordLocalMachineConfig.mainLocation);
    return {};
}

tempo_utils::Status
make_local_machine(
    std::shared_ptr<LocalMachine> &localMachine,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    std::shared_ptr<lyric_runtime::InterpreterState> interpreterState,
    AbstractMessageSender<RunnerReply> *processor)
{
    localMachine = componentConstructor.createLocalMachine(
        chordLocalMachineConfig.machineUrl, chordLocalMachineConfig.startSuspended, interpreterState, processor);
    return {};
}

tempo_utils::Status
make_remoting_service(
    std::unique_ptr<RemotingService> &remotingService,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    std::shared_ptr<LocalMachine> localMachine,
    uv_async_t *initComplete)
{
    remotingService = componentConstructor.createRemotingService(
        chordLocalMachineConfig.startSuspended, localMachine, initComplete);
    return {};
}

tempo_utils::Status
make_custom_channel(
    std::shared_ptr<grpc::ChannelInterface> &channel,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig)
{
    tempo_utils::FileReader rootCABundleReader(chordLocalMachineConfig.pemRootCABundleFile);
    if (!rootCABundleReader.isValid())
        return rootCABundleReader.getStatus();

    grpc::SslCredentialsOptions options;
    auto rootCABytes = rootCABundleReader.getBytes();
    options.pem_root_certs = std::string((const char *) rootCABytes->getData(), rootCABytes->getSize());
    auto credentials = grpc::SslCredentials(options);

    grpc::ChannelArguments channelArguments;
    if (!chordLocalMachineConfig.supervisorNameOverride.empty()) {
        TU_LOG_INFO << "using target name override " << chordLocalMachineConfig.supervisorNameOverride
            << " for supervisor endpoint " << chordLocalMachineConfig.supervisorUrl;
        channelArguments.SetSslTargetNameOverride(chordLocalMachineConfig.supervisorNameOverride);
    }

    channel = componentConstructor.createCustomChannel(
        chordLocalMachineConfig.supervisorUrl.toString(), credentials, channelArguments);
    return {};
}

tempo_utils::Status
make_invoke_service_stub(
    std::unique_ptr<chord_invoke::InvokeService::StubInterface> &stub,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    std::shared_ptr<grpc::ChannelInterface> &customChannel)
{
    stub = componentConstructor.createInvokeStub(customChannel);
    return {};
}

tempo_utils::Status
make_csr_key_pair(
    tempo_security::CSRKeyPair &csrKeyPair,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig)
{
    tempo_security::ECCPrivateKeyGenerator keygen(NID_X9_62_prime256v1);

//    auto commonName = chordLocalMachineConfig.machineUrl.toString();
//    if (!chordLocalMachineConfig.machineNameOverride.empty()) {
//        commonName = chordLocalMachineConfig.machineNameOverride;
//    }
    TU_LOG_INFO << "machine url: " << chordLocalMachineConfig.machineUrl;
    TU_LOG_INFO << "machine host: " << chordLocalMachineConfig.machineUrl.getHost();
    TU_LOG_INFO << "machine host and port: " << chordLocalMachineConfig.machineUrl.getHostAndPort();
    std::string commonName = chordLocalMachineConfig.machineUrl.getHost();
    TU_LOG_INFO << "using binder common name " << commonName;

    TU_ASSIGN_OR_RETURN (csrKeyPair, tempo_security::generate_csr_key_pair(
        keygen, chordLocalMachineConfig.binderOrganization, chordLocalMachineConfig.binderOrganizationalUnit,
        commonName, chordLocalMachineConfig.runDirectory, chordLocalMachineConfig.binderCsrFilenameStem));

    std::shared_ptr<tempo_security::X509CertificateSigningRequest> req;
    TU_ASSIGN_OR_RETURN (req, tempo_security::X509CertificateSigningRequest::readFile(csrKeyPair.getPemRequestFile()));
    TU_LOG_INFO << "csr req: " << req->toString();

    return {};
}

tempo_utils::Status
make_grpc_binder(
    std::shared_ptr<GrpcBinder> &binder,
    const ComponentConstructor &componentConstructor,
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    const tempo_security::CSRKeyPair &csrKeyPair,
    chord_invoke::InvokeService::StubInterface *invokeStub,
    RemotingService *remotingService)
{
    lyric_common::RuntimePolicy policy;
    binder = componentConstructor.createGrpcBinder(chordLocalMachineConfig.binderEndpoint,
        policy, csrKeyPair.getPemPrivateKeyFile(), chordLocalMachineConfig.pemRootCABundleFile,
        remotingService);
    return {};
}
