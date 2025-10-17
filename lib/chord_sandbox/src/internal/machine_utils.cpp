
#include <absl/strings/ascii.h>

#include <chord_invoke/invoke_service.grpc.pb.h>
#include <chord_sandbox/internal/machine_utils.h>
#include <tempo_config/config_utils.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_security/x509_certificate_signing_request.h>

tempo_utils::Result<chord_sandbox::internal::CreateMachineResult>
chord_sandbox::internal::create_machine(
    chord_invoke::InvokeService::StubInterface *stub,
    std::string_view name,
    const tempo_utils::Url &executionUrl,
    const tempo_config::ConfigMap &configMap,
    const absl::flat_hash_set<chord_common::RequestedPort> &requestedPorts,
    bool startSuspended)
{
    grpc::ClientContext createMachineContext;
    chord_invoke::CreateMachineRequest createMachineRequest;
    chord_invoke::CreateMachineResult createMachineResult;
    CreateMachineResult result;

    // set the name of the machine
    createMachineRequest.set_name(name);

    // set the execution uri to the main location
    createMachineRequest.set_execution_url(executionUrl.toString());

    // set flag indicating whether to start the remote machine in suspended state
    createMachineRequest.set_start_suspended(startSuspended);

    // add a requested port for each given port parameter
    for (const auto &requestedPort : requestedPorts) {
        auto protocolUrl = requestedPort.getUrl();
        auto *requestedPortPtr = createMachineRequest.add_requested_ports();
        requestedPortPtr->set_protocol_url(protocolUrl.toString());
        switch (requestedPort.getType()) {
            case chord_common::PortType::OneShot:
                requestedPortPtr->set_port_type(chord_invoke::OneShot);
                break;
            case chord_common::PortType::Streaming:
                requestedPortPtr->set_port_type(chord_invoke::Streaming);
                break;
            default:
                return SandboxStatus::forCondition(
                    SandboxCondition::kInvalidConfiguration, "invalid port type");
        }
        switch (requestedPort.getDirection()) {
            case chord_common::PortDirection::Client:
                requestedPortPtr->set_port_direction(chord_invoke::Client);
                break;
            case chord_common::PortDirection::Server:
                requestedPortPtr->set_port_direction(chord_invoke::Server);
                break;
            case chord_common::PortDirection::BiDirectional:
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
    TU_LOG_INFO << "created machine " << createMachineResult.machine_url();

    // copy the machine url into result
    result.machineUrl = tempo_utils::Url::fromString(createMachineResult.machine_url());

    // copy the control url into result if one was returned
    if (createMachineResult.control_endpoint_index() >= 0) {
        auto &declaredEndpoint = createMachineResult.declared_endpoints(
            createMachineResult.control_endpoint_index());
        result.controlUrl = tempo_utils::Url::fromString(declaredEndpoint.endpoint_url());
    }

    auto &declaredEndpoints = createMachineResult.declared_endpoints();

    // build the map of protocol url to endpoint url
    for (const auto &declaredPort : createMachineResult.declared_ports()) {
        auto protocolUrl = tempo_utils::Url::fromString(declaredPort.protocol_url());
        if (declaredEndpoints.size() <= declaredPort.endpoint_index())
            return SandboxStatus::forCondition(
                SandboxCondition::kInvalidConfiguration, "invalid endpoint index");
        auto &declaredEndpoint = declaredEndpoints.at(declaredPort.endpoint_index());
        auto endpointUrl = tempo_utils::Url::fromString(declaredEndpoint.endpoint_url());
        result.protocolEndpoints[protocolUrl] = endpointUrl;
    }

    // build the map of endpoint url to CSR
    for (const auto &declaredEndpoint : createMachineResult.declared_endpoints()) {
        auto endpointUrl = tempo_utils::Url::fromString(declaredEndpoint.endpoint_url());
        result.endpointCsrs[endpointUrl] = declaredEndpoint.csr();
    }

    return result;
}

tempo_utils::Result<chord_sandbox::internal::RunMachineResult>
chord_sandbox::internal::run_machine(
        chord_invoke::InvokeService::StubInterface *stub,
        const tempo_utils::Url &machineUrl,
        const absl::flat_hash_map<tempo_utils::Url, tempo_utils::Url> &protocolEndpoints,
        const absl::flat_hash_map<tempo_utils::Url,std::string> &endpointCsrs,
        const tempo_security::CertificateKeyPair &caKeyPair,
        std::chrono::seconds certificateValidity)
{
    grpc::ClientContext runMachineContext;
    chord_invoke::RunMachineRequest runMachineRequest;
    chord_invoke::RunMachineResult runMachineResult;
    RunMachineResult result;

    // set the machine uri returned from CreateMachine
    runMachineRequest.set_machine_url(machineUrl.toString());

    // sign the csr for each endpoint
    for (const auto &entry : endpointCsrs) {
        auto *signedEndpoint = runMachineRequest.add_signed_endpoints();

        signedEndpoint->set_endpoint_url(entry.first.toString());

        const auto &pemRequestBytes = entry.second;
        auto generateCertificateResult = tempo_security::generate_certificate_from_csr(
            pemRequestBytes, caKeyPair, 1, certificateValidity);
        if (generateCertificateResult.isStatus())
            return generateCertificateResult.getStatus();
        auto pemCertificateBytes = generateCertificateResult.getResult();
        signedEndpoint->set_certificate(pemCertificateBytes);

        std::shared_ptr<tempo_security::X509Certificate> cert;
        TU_ASSIGN_OR_RETURN (cert, tempo_security::X509Certificate::fromString(pemCertificateBytes));
        result.endpointNameOverrides[entry.first] = cert->getCommonName();
    }

    // call RunMachine on the sandbox-agent
    auto status = stub->RunMachine(&runMachineContext, runMachineRequest, &runMachineResult);
    if (!status.ok())
        return SandboxStatus::forCondition(SandboxCondition::kAgentError,
            "RunMachine failed: {}", status.error_message());

    TU_LOG_INFO << "started machine " << machineUrl;

    // build set of bound endpoint urls
    absl::flat_hash_set<tempo_utils::Url> boundEndpoints;
    for (const auto &boundEndpoint : runMachineResult.bound_endpoints()) {
        auto endpointUrl = tempo_utils::Url::fromString(boundEndpoint.endpoint_url());
        boundEndpoints.insert(endpointUrl);
    }

    // ensure every endpoint is bound
    for (const auto &entry : endpointCsrs) {
        if (!boundEndpoints.contains(entry.first))
            return SandboxStatus::forCondition(SandboxCondition::kAgentError,
                "RunMachine failed: endpoint {} was not bound", entry.first.toString());
    }

    return result;
}