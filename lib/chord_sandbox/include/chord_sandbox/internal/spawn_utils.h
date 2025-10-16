#ifndef CHORD_SANDBOX_INTERNAL_SPAWN_UTILS_H
#define CHORD_SANDBOX_INTERNAL_SPAWN_UTILS_H

#include <chord_sandbox/chord_isolate.h>
#include <tempo_utils/process_runner.h>
#include <tempo_utils/status.h>

namespace chord_sandbox::internal {

    // typedef std::shared_ptr<tempo_utils::DaemonProcess> (*SpawnFunc)(
    //     const tempo_utils::ProcessInvoker &,
    //     const std::filesystem::path &);

    using SpawnFunc = std::function<
        std::shared_ptr<tempo_utils::ProcessRunner>(
        const tempo_utils::ProcessInvoker &,
        const std::filesystem::path &)>;

    struct AgentParams {
        std::shared_ptr<tempo_utils::ProcessRunner> agentProcess;
        std::string agentEndpoint;
        std::string agentServerName;
        std::filesystem::path pemRootCABundleFile;
    };

    tempo_utils::Status spawn_temporary_agent(
        AgentParams &params,
        const std::filesystem::path &agentPath,
        const std::string &agentServerName,
        const std::filesystem::path &runDirectory,
        absl::Duration idleTimeout,
        const std::filesystem::path &pemCertificateFile,
        const std::filesystem::path &pemPrivateKeyFile,
        const std::filesystem::path &pemRootCABundleFile);

    tempo_utils::Status connect_to_specified_endpoint(
        AgentParams &params,
        const tempo_utils::Url &agentEndpoint,
        const std::string &agentServerName,
        const std::filesystem::path &pemRootCABundleFile);

    tempo_utils::Status spawn_temporary_agent_if_missing(
        AgentParams &params,
        const tempo_utils::Url &agentEndpoint,
        const std::filesystem::path &agentPath,
        chord_protocol::TransportType transport,
        const std::string &agentServerName,
        const std::filesystem::path &runDirectory,
        const std::filesystem::path &pemCertificateFile,
        const std::filesystem::path &pemPrivateKeyFile,
        const std::filesystem::path &pemRootCABundleFile);

}

#endif // CHORD_SANDBOX_INTERNAL_SPAWN_UTILS_H
