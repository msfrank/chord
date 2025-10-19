#ifndef CHORD_SANDBOX_INTERNAL_SPAWN_UTILS_H
#define CHORD_SANDBOX_INTERNAL_SPAWN_UTILS_H

#include <chord_sandbox/chord_isolate.h>
#include <tempo_utils/process_runner.h>
#include <tempo_utils/status.h>

namespace chord_sandbox::internal {

    struct SpawnAgentResult {
        std::shared_ptr<tempo_utils::ProcessRunner> process;
        std::string endpoint;
    };

    tempo_utils::Result<SpawnAgentResult> spawn_agent(
        const std::filesystem::path &agentPath,
        const std::string &agentServerName,
        const std::filesystem::path &runDirectory,
        absl::Duration idleTimeout,
        const std::filesystem::path &pemCertificateFile,
        const std::filesystem::path &pemPrivateKeyFile,
        const std::filesystem::path &pemRootCABundleFile);

}

#endif // CHORD_SANDBOX_INTERNAL_SPAWN_UTILS_H
