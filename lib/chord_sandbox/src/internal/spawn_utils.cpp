
#include <absl/strings/ascii.h>

#include <chord_sandbox/internal/spawn_utils.h>
#include <tempo_utils/process_builder.h>

tempo_utils::Result<chord_sandbox::internal::SpawnAgentResult>
chord_sandbox::internal::spawn_agent(
    const std::filesystem::path &agentPath,
    const std::string &agentServerName,
    const std::filesystem::path &runDirectory,
    absl::Duration idleTimeout,
    const std::filesystem::path &pemCertificateFile,
    const std::filesystem::path &pemPrivateKeyFile,
    const std::filesystem::path &pemRootCABundleFile)
{
    SpawnAgentResult spawnAgentResult;

    auto agentSock = runDirectory / "agent.sock";
    spawnAgentResult.endpoint = absl::StrCat("unix:", agentSock.c_str());

    std::filesystem::path exepath;
    if (!agentPath.empty()) {
        exepath = agentPath;
    } else {
        exepath = CHORD_AGENT_EXECUTABLE;
    }

    tempo_utils::ProcessBuilder builder(exepath);
    builder.appendArg("--agent-name", agentServerName);
    builder.appendArg("--temporary-session");
    builder.appendArg("--listen-transport", "unix");
    builder.appendArg("--background");
    builder.appendArg("--certificate", pemCertificateFile.string());
    builder.appendArg("--private-key", pemPrivateKeyFile.string());
    builder.appendArg("--ca-bundle", pemRootCABundleFile.string());
    auto idleTimeoutInSeconds = absl::ToInt64Seconds(idleTimeout);
    if (idleTimeoutInSeconds > 0) {
        builder.appendArg("--idle-timeout", absl::StrCat(idleTimeoutInSeconds));
    }
    auto invoker = builder.toInvoker();
    TU_LOG_INFO << "invoking " << invoker.toString();

    spawnAgentResult.process = std::make_shared<tempo_utils::ProcessRunner>(invoker, runDirectory);
    TU_RETURN_IF_NOT_OK (spawnAgentResult.process->getStatus());

    return spawnAgentResult;
}