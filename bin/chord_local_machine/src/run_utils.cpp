
#include <chord_local_machine/port_socket.h>
#include <chord_local_machine/run_utils.h>
#include <tempo_command/command_result.h>
#include <tempo_utils/file_reader.h>
#include <tempo_utils/tempfile_maker.h>

tempo_utils::Status
sign_certificates(
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    ChordLocalMachineData &chordLocalMachineData)
{
    auto pemRequestFile = chordLocalMachineData.csrKeyPair.getPemRequestFile();

    // read the CSR
    tempo_utils::FileReader csrReader(pemRequestFile);
    if (!csrReader.isValid())
        return csrReader.getStatus();
    auto csrBytes = csrReader.getBytes();

    // register the interpreter with the supervisor
    grpc::ClientContext signCertificatesContext;
    chord_invoke::SignCertificatesRequest signCertificatesRequest;
    chord_invoke::SignCertificatesResult signCertificatesResult;

    //
    signCertificatesRequest.set_machine_url(chordLocalMachineConfig.machineUrl.toString());

    //
    auto *declaredEndpoint = signCertificatesRequest.add_declared_endpoints();
    declaredEndpoint->set_endpoint_url(chordLocalMachineConfig.binderEndpoint);
    declaredEndpoint->set_csr(std::string((const char *) csrBytes->getData(), csrBytes->getSize()));

    //
    for (const auto &expectedPort : chordLocalMachineConfig.expectedPorts) {
        auto *declaredPort = signCertificatesRequest.add_declared_ports();
        declaredPort->set_protocol_url(expectedPort.toString());
        declaredPort->set_endpoint_index(0);
    }

    //
    TU_LOG_INFO << "requesting certificate signing for " << chordLocalMachineConfig.machineUrl;
    auto signCertificatesStatus = chordLocalMachineData.invokeStub->SignCertificates(
        &signCertificatesContext, signCertificatesRequest, &signCertificatesResult);
    TU_LOG_ERROR_IF(!signCertificatesStatus.ok()) << "SignCertificates failed: "
        << signCertificatesStatus.error_message();
    if (!signCertificatesStatus.ok())
        return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
            "certificate signing failure: {}", signCertificatesStatus.error_message());

    TU_ASSERT (signCertificatesResult.signed_endpoints_size() == 1);

    auto signedEndpoint = signCertificatesResult.signed_endpoints(0);
    TU_LOG_INFO << "received certificate for " << signedEndpoint.endpoint_url();

    tempo_utils::TempfileMaker certWriter(chordLocalMachineConfig.runDirectory,
        "signed-cert.XXXXXXXX", signedEndpoint.certificate());
    if (!certWriter.isValid())
        return certWriter.getStatus();

    chordLocalMachineData.pemCertificateFile = certWriter.getTempfile();
    TU_LOG_INFO << "stored certificate in " << chordLocalMachineData.pemCertificateFile;

    return {};
}

tempo_utils::Status
register_protocols(
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    ChordLocalMachineData &chordLocalMachineData)
{
    auto expectedPorts = chordLocalMachineConfig.expectedPorts;

//    // register the run protocol if socket exists
//    if (chordLocalMachineData.runSocket != nullptr) {
//        auto protocolUrl = chordLocalMachineData.runSocket->getProtocolUri();
//        TU_ASSERT (expectedPorts.contains(protocolUrl));
//        TU_RETURN_IF_NOT_OK (chordLocalMachineData.remotingService->registerProtocolHandler(protocolUrl,
//            chordLocalMachineData.runSocket, /* requiredAtLaunch= */ true));
//        TU_LOG_INFO << "registered expected port " << protocolUrl;
//        expectedPorts.erase(protocolUrl);
//    }

    // register the remaining expected ports
    auto *multiplexer = chordLocalMachineData.interpreterState->portMultiplexer();
    for (const auto &expectedPort : expectedPorts) {
        std::shared_ptr<lyric_runtime::DuplexPort> duplexPort;
        TU_ASSIGN_OR_RETURN (duplexPort, multiplexer->registerPort(expectedPort));
        auto socket = std::make_shared<PortSocket>(duplexPort);
        chordLocalMachineData.remotingService->registerProtocolHandler(expectedPort,
            socket, /* requiredAtLaunch= */ true);
        TU_LOG_INFO << "registered expected port " << expectedPort;
    }

    // start the binder
    TU_RETURN_IF_NOT_OK (
        chordLocalMachineData.grpcBinder->initialize(chordLocalMachineData.pemCertificateFile));
    return {};
}

tempo_utils::Status advertise_endpoints(
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    ChordLocalMachineData &chordLocalMachineData)
{
    // register the interpreter with the supervisor
    grpc::ClientContext advertiseEndpointsContext;
    chord_invoke::AdvertiseEndpointsRequest advertiseEndpointsRequest;
    chord_invoke::AdvertiseEndpointsResult advertiseEndpointsResult;

    auto machineUrl = chordLocalMachineData.localMachine->getMachineUrl();
    advertiseEndpointsRequest.set_machine_url(machineUrl.toString());
    auto *boundEndpoint = advertiseEndpointsRequest.add_bound_endpoints();
    boundEndpoint->set_endpoint_url(chordLocalMachineConfig.binderEndpoint);

    TU_LOG_INFO << "advertising endpoint for " << machineUrl;
    auto advertiseEndpointsStatus = chordLocalMachineData.invokeStub->AdvertiseEndpoints(&advertiseEndpointsContext,
        advertiseEndpointsRequest, &advertiseEndpointsResult);
    TU_LOG_ERROR_IF(!advertiseEndpointsStatus.ok()) << "AdvertiseEndpoints failed: "
        << advertiseEndpointsStatus.error_message();
    if (!advertiseEndpointsStatus.ok())
        return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
            advertiseEndpointsStatus.error_message());

    TU_LOG_INFO << "endpoint is advertised";

    return {};
}

tempo_utils::Status
run_local_machine(
    const ChordLocalMachineConfig &chordLocalMachineConfig,
    ChordLocalMachineData &chordLocalMachineData)
{
    // create the certificate signing requests and send them to the agent
    TU_RETURN_IF_NOT_OK (sign_certificates(chordLocalMachineConfig, chordLocalMachineData));

    // register a handler for each requested port
    TU_RETURN_IF_NOT_OK (register_protocols(chordLocalMachineConfig, chordLocalMachineData));

    // send the endpoint advertisements to the agent
    TU_RETURN_IF_NOT_OK (advertise_endpoints(chordLocalMachineConfig, chordLocalMachineData));

    // if there are no expected ports then signal the local machine that init is complete
    if (chordLocalMachineConfig.expectedPorts.empty()) {
        chordLocalMachineData.localMachine->notifyInitComplete();
    }

    // pass control to uv loop and wait for signal or shutdown message from the supervisor
    TU_LOG_V << "uv loop running";
    auto ret = uv_run(&chordLocalMachineData.mainLoop, UV_RUN_DEFAULT);
    TU_LOG_V << "uv loop stopped";

    if (ret != 0) {
        TU_LOG_V << "detected open handles after stopping uv loop:";
        uv_print_all_handles(&chordLocalMachineData.mainLoop, tempo_utils::get_logging_sink());
    }
    return {};
}
