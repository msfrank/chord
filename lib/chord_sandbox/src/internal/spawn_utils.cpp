
#include <absl/strings/ascii.h>

#include <chord_sandbox/internal/spawn_utils.h>
#include <tempo_utils/process_builder.h>

tempo_utils::Status
chord_sandbox::internal::spawn_temporary_agent(
    AgentParams &params,
    const std::filesystem::path &agentPath,
    const std::string &agentServerName,
    const std::filesystem::path &runDirectory,
    absl::Duration idleTimeout,
    const std::filesystem::path &pemCertificateFile,
    const std::filesystem::path &pemPrivateKeyFile,
    const std::filesystem::path &pemRootCABundleFile)
{
    auto agentSock = runDirectory / "agent.sock";
    auto agentEndpoint = absl::StrCat("unix:", agentSock.c_str());
    auto idleTimeoutInSeconds = absl::ToInt64Seconds(idleTimeout);

    tempo_utils::ProcessBuilder builder(agentPath);
    builder.appendArg("--agent-name", agentServerName);
    builder.appendArg("--temporary-session");
    builder.appendArg("--listen-transport", "unix");
    builder.appendArg("--background");
    builder.appendArg("--certificate", pemCertificateFile.string());
    builder.appendArg("--private-key", pemPrivateKeyFile.string());
    builder.appendArg("--ca-bundle", pemRootCABundleFile.string());
    if (idleTimeoutInSeconds > 0) {
        builder.appendArg("--idle-timeout", absl::StrCat(idleTimeoutInSeconds));
    }
    auto invoker = builder.toInvoker();
    TU_LOG_INFO << "invoking " << invoker.toString();

    auto agentProcess = std::make_shared<tempo_utils::ProcessRunner>(invoker, runDirectory);
    TU_RETURN_IF_NOT_OK (agentProcess->getStatus());

    TU_LOG_INFO << "agent endpoint is " << agentEndpoint;

    if (agentEndpoint.empty())
        return SandboxStatus::forCondition(
            SandboxCondition::kAgentError, "missing agent endpoint");

    // update the agent params
    params.agentProcess = agentProcess;
    params.agentEndpoint = agentEndpoint;
    params.agentServerName = agentServerName;
    params.pemRootCABundleFile = pemRootCABundleFile;

    uv_sleep(3003);

    return {};
}

tempo_utils::Status
chord_sandbox::internal::connect_to_specified_endpoint(
    AgentParams &params,
    const tempo_utils::Url &agentEndpoint,
    const std::string &agentServerName,
    const std::filesystem::path &pemRootCABundleFile)
{
    return tempo_utils::GenericStatus::forCondition(
        tempo_utils::GenericCondition::kUnimplemented, "connect_to_specified_endpoint");
}

tempo_utils::Status
chord_sandbox::internal::spawn_temporary_agent_if_missing(
    AgentParams &params,
    const tempo_utils::Url &agentEndpoint,
    const std::filesystem::path &agentPath,
    chord_protocol::TransportType transport,
    const std::string &agentServerName,
    const std::filesystem::path &runDirectory,
    const std::filesystem::path &pemCertificateFile,
    const std::filesystem::path &pemPrivateKeyFile,
    const std::filesystem::path &pemRootCABundleFile)
{
    return tempo_utils::GenericStatus::forCondition(
        tempo_utils::GenericCondition::kUnimplemented, "spawn_temporary_agent_if_missing");
}