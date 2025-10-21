#ifndef CHORD_SANDBOX_INTERNAL_MACHINE_UTILS_H
#define CHORD_SANDBOX_INTERNAL_MACHINE_UTILS_H

#include <chord_common/abstract_certificate_signer.h>
#include <chord_sandbox/chord_isolate.h>

namespace chord_sandbox::internal {

    /**
     * The result from calling create_machine.
     */
    struct CreateMachineResult {
        tempo_utils::Url machineUrl;                /**< Url which uniquely identifies the machine. */
        tempo_utils::Url controlUrl;                /**< Url of the control endpoint. */
        absl::flat_hash_map<
            tempo_utils::Url,
            tempo_utils::Url> protocolEndpoints;    /**< Map of protocol url to endpoint url. */
        absl::flat_hash_map<
            tempo_utils::Url,
            std::string> endpointCsrs;              /**< Map of endpoint url to certificate signing request. */
    };

    /**
     * The result from calling run_machine.
     */
    struct RunMachineResult {
        absl::flat_hash_map<
            tempo_utils::Url,
            std::string> endpointNameOverrides;
    };

    tempo_utils::Result<CreateMachineResult> create_machine(
        chord_invoke::InvokeService::StubInterface *stub,
        std::string_view name,
        const tempo_utils::Url &executionUrl,
        const tempo_config::ConfigMap &configMap,
        const absl::flat_hash_set<chord_common::RequestedPort> &requestedPorts,
        bool startSuspended);

    tempo_utils::Result<RunMachineResult> run_machine(
        chord_invoke::InvokeService::StubInterface *stub,
        const tempo_utils::Url &machineUrl,
        const absl::flat_hash_map<tempo_utils::Url, tempo_utils::Url> &protocolEndpoints,
        const absl::flat_hash_map<tempo_utils::Url,std::string> &endpointCsrs,
        std::shared_ptr<chord_common::AbstractCertificateSigner> certificateSigner,
        absl::Duration requestedValidityPeriod);

}

#endif // CHORD_SANDBOX_INTERNAL_MACHINE_UTILS_H
