
#include <absl/strings/ascii.h>

#include <chord_invoke/invoke_service.grpc.pb.h>
#include <chord_sandbox/internal/machine_utils.h>
#include <tempo_config/config_serde.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_security/x509_certificate_signing_request.h>

tempo_utils::Result<chord_invoke::CreateMachineResult>
chord_sandbox::internal::create_machine(
    chord_invoke::InvokeService::StubInterface *stub,
    std::string_view name,
    const tempo_utils::Url &executionUrl,
    const tempo_config::ConfigMap &configMap,
    const absl::flat_hash_set<chord_protocol::RequestedPort> &requestedPorts)
{
    grpc::ClientContext createMachineContext;
    chord_invoke::CreateMachineRequest createMachineRequest;
    chord_invoke::CreateMachineResult createMachineResult;

    // set the name of the machine
    createMachineRequest.set_name(name);

    // set the execution uri to the main location
    createMachineRequest.set_execution_uri(executionUrl.toString());

    // add a requested port for each given port parameter
    for (const auto &requestedPort : requestedPorts) {
        auto protocolUrl = requestedPort.getUrl();
        auto *requestedPortPtr = createMachineRequest.add_requested_ports();
        requestedPortPtr->set_protocol_uri(protocolUrl.toString());
        switch (requestedPort.getType()) {
            case chord_protocol::PortType::OneShot:
                requestedPortPtr->set_port_type(chord_invoke::OneShot);
                break;
            case chord_protocol::PortType::Streaming:
                requestedPortPtr->set_port_type(chord_invoke::Streaming);
                break;
            default:
                return SandboxStatus::forCondition(
                    SandboxCondition::kInvalidConfiguration, "invalid port type");
        }
        switch (requestedPort.getDirection()) {
            case chord_protocol::PortDirection::Client:
                requestedPortPtr->set_port_direction(chord_invoke::Client);
                break;
            case chord_protocol::PortDirection::Server:
                requestedPortPtr->set_port_direction(chord_invoke::Server);
                break;
            case chord_protocol::PortDirection::BiDirectional:
                requestedPortPtr->set_port_direction(chord_invoke::BiDirectional);
                break;
            default:
                return SandboxStatus::forCondition(
                    SandboxCondition::kInvalidConfiguration, "invalid port direction");
        }
    }

    // set the config hash from the serialized config map
    std::string config;
    tempo_config::write_config_string(configMap, config);
    createMachineRequest.set_config_hash(config);

    // call CreateMachine on the sandbox-agent
    auto status = stub->CreateMachine(&createMachineContext, createMachineRequest, &createMachineResult);
    if (!status.ok())
        return SandboxStatus::forCondition(SandboxCondition::kAgentError,
            "CreateMachine failed: {}", status.error_message());
    TU_LOG_INFO << "created machine " << createMachineResult.machine_uri();

    return createMachineResult;
}

tempo_utils::Result<chord_invoke::RunMachineResult>
chord_sandbox::internal::run_machine(
        chord_invoke::InvokeService::StubInterface *stub,
        const tempo_utils::Url &machineUrl,
        const absl::flat_hash_map<std::string,std::string> &declaredEndpointCsrs,
        const tempo_security::CertificateKeyPair &caKeyPair,
        std::chrono::seconds certificateValidity)
{
    grpc::ClientContext runMachineContext;
    chord_invoke::RunMachineRequest runMachineRequest;
    chord_invoke::RunMachineResult runMachineResult;

    // set the machine uri returned from CreateMachine
    runMachineRequest.set_machine_uri(machineUrl.toString());

    // sign the csr for each endpoint
    for (const auto &entry : declaredEndpointCsrs) {
        auto *signedEndpoint = runMachineRequest.add_signed_endpoints();

        signedEndpoint->set_endpoint_uri(entry.first);

        const auto &pemRequestBytes = entry.second;
        auto generateCertificateResult = tempo_security::generate_certificate_from_csr(
            pemRequestBytes, caKeyPair, 1, certificateValidity);
        if (generateCertificateResult.isStatus())
            return generateCertificateResult.getStatus();
        signedEndpoint->set_certificate(generateCertificateResult.getResult());
    }

    // call RunMachine on the sandbox-agent
    auto status = stub->RunMachine(&runMachineContext, runMachineRequest, &runMachineResult);
    if (!status.ok())
        return SandboxStatus::forCondition(SandboxCondition::kAgentError,
            "RunMachine failed: {}", status.error_message());
    TU_LOG_INFO << "started machine " << machineUrl;

    return runMachineResult;
}