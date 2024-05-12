
#include <absl/strings/ascii.h>

#include <chord_sandbox/internal/spawn_utils.h>
#include <tempo_config/config_serde.h>

tempo_utils::Status
chord_sandbox::internal::spawn_temporary_agent(
    AgentParams &params,
    const std::filesystem::path &agentPath,
    chord_protocol::TransportType transport,
    const std::string &agentServerName,
    const std::filesystem::path &runDirectory,
    const std::filesystem::path &pemCertificateFile,
    const std::filesystem::path &pemPrivateKeyFile,
    const std::filesystem::path &pemRootCABundleFile,
    SpawnFunc spawnFunc)
{
    TU_ASSERT (spawnFunc != nullptr);

    std::string transportType;
    switch (transport) {
        case chord_protocol::TransportType::Unix:
            transportType = "unix";
            break;
        case chord_protocol::TransportType::Tcp:
            transportType = "tcp";
            break;
        default:
            return SandboxStatus::forCondition(
                SandboxCondition::kInvalidConfiguration, "invalid transport");
    }

    tempo_utils::ProcessBuilder builder(agentPath);
    builder.appendArg("--agent-name", agentServerName);
    builder.appendArg("--temporary-session");
    builder.appendArg("--listen-transport", transportType);
    builder.appendArg("--emit-endpoint");
    builder.appendArg("--idle-timeout", "5");
    builder.appendArg("--background");
    builder.appendArg("--certificate", pemCertificateFile.string());
    builder.appendArg("--private-key", pemPrivateKeyFile.string());
    builder.appendArg("--ca-bundle", pemRootCABundleFile.string());
    auto invoker = builder.toInvoker();

    auto agentProcess = spawnFunc(invoker, runDirectory);
    if (agentProcess == nullptr)
        return SandboxStatus::forCondition(
            SandboxCondition::kAgentError, "failed to spawn agent");

    auto agentEndpoint = agentProcess->getChildOutput();
    absl::StripAsciiWhitespace(&agentEndpoint);
    TU_LOG_INFO << "agent endpoint is " << agentEndpoint;

    if (agentEndpoint.empty())
        return SandboxStatus::forCondition(
            SandboxCondition::kAgentError, "missing agent endpoint");

    // update the agent params
    params.agentProcess = agentProcess;
    params.agentEndpoint = agentEndpoint;
    params.agentServerName = agentServerName;
    params.pemRootCABundleFile = pemRootCABundleFile;

    return SandboxStatus::ok();
}

tempo_utils::Status
chord_sandbox::internal::connect_to_specified_endpoint(
    AgentParams &params,
    const tempo_utils::Url &agentEndpoint,
    const std::string &agentServerName,
    const std::filesystem::path &pemRootCABundleFile,
    SpawnFunc spawnFunc)
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
    const std::filesystem::path &pemRootCABundleFile,
    SpawnFunc spawnFunc)
{
    return tempo_utils::GenericStatus::forCondition(
        tempo_utils::GenericCondition::kUnimplemented, "spawn_temporary_agent_if_missing");
}