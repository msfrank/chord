#ifndef CHORD_SANDBOX_INTERNAL_SESSION_UTILS_H
#define CHORD_SANDBOX_INTERNAL_SESSION_UTILS_H

#include <chord_sandbox/chord_isolate.h>
#include <tempo_utils/process_runner.h>
#include <tempo_utils/status.h>

namespace chord_sandbox::internal {

    struct PrepareSessionResult {
        std::string sessionId;
        std::string commonName;
        std::filesystem::path pemCertificateFile;
        std::filesystem::path pemPrivateKeyFile;
    };

    struct SpawnSessionResult {
        std::shared_ptr<tempo_utils::ProcessRunner> process;
        chord_common::TransportLocation endpoint;
    };

    struct LoadSessionResult {
        std::string processId;
        std::string sessionId;
        chord_common::TransportLocation endpoint;
    };

    tempo_utils::Result<PrepareSessionResult> prepare_session(
        const std::filesystem::path &sessionDirectory,
        const std::filesystem::path &pemRootCABundleFile,
        std::shared_ptr<chord_common::AbstractCertificateSigner> certificateSigner);

    tempo_utils::Result<SpawnSessionResult> spawn_session(
        const std::filesystem::path &sessionDirectory,
        const std::filesystem::path &agentPath,
        const std::filesystem::path &pemRootCABundleFile,
        const std::filesystem::path &pemAgentCertificateFile,
        const std::filesystem::path &pemAgentPrivateKeyFile,
        absl::Duration idleTimeout,
        absl::Duration registrationTimeout);

    tempo_utils::Result<LoadSessionResult> load_session(const std::filesystem::path &sessionDirectory);

    tempo_utils::Result<chord_common::TransportLocation> load_session_endpoint(
        const std::filesystem::path &sessionDirectory,
        absl::Duration timeout);
}

#endif // CHORD_SANDBOX_INTERNAL_SESSION_UTILS_H
