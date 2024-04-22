#ifndef CHORD_SANDBOX_INTERNAL_MACHINE_UTILS_H
#define CHORD_SANDBOX_INTERNAL_MACHINE_UTILS_H

#include <chord_sandbox/chord_isolate.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_utils/daemon_process.h>
#include <tempo_utils/status.h>

namespace chord_sandbox::internal {

    tempo_utils::Result<chord_invoke::CreateMachineResult> create_machine(
        chord_invoke::InvokeService::StubInterface *stub,
        std::string_view name,
        const tempo_utils::Url &executionUrl,
        const tempo_config::ConfigMap &configMap,
        const absl::flat_hash_set<chord_protocol::RequestedPort> &requestedPorts);

    tempo_utils::Result<chord_invoke::RunMachineResult> run_machine(
        chord_invoke::InvokeService::StubInterface *stub,
        const tempo_utils::Url &machineUrl,
        const absl::flat_hash_map<std::string,std::string> &declaredEndpointCsrs,
        const tempo_security::CertificateKeyPair &caKeyPair,
        std::chrono::seconds certificateValidity);

}

#endif // CHORD_SANDBOX_INTERNAL_MACHINE_UTILS_H
