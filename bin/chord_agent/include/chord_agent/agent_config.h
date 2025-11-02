#ifndef CHORD_AGENT_AGENT_CONFIG_H
#define CHORD_AGENT_AGENT_CONFIG_H

#include <string>
#include <filesystem>

#include <chord_common/transport_location.h>
#include <tempo_command/command_config.h>
#include <tempo_utils/status.h>

namespace chord_agent {

    constexpr const char * kAgentCertificateFileName    = "agent.crt";
    constexpr const char * kAgentPrivateKeyFileName     = "agent.key";
    constexpr const char * kRootCABundleFileName        = "rootca.crt";

    struct AgentConfig {
        std::string sessionName;
        chord_common::TransportLocation listenLocation;
        std::filesystem::path runDirectory;
        std::filesystem::path machineExecutable;
        std::filesystem::path pemCertificateFile;
        std::filesystem::path pemPrivateKeyFile;
        std::filesystem::path pemRootCABundleFile;
        bool runInBackground;
        bool temporarySession;
        absl::Duration idleTimeout;
        absl::Duration registrationTimeout;
        std::filesystem::path logFile;
        std::filesystem::path pidFile;
        std::filesystem::path endpointFile;
    };

    tempo_utils::Status configure_agent(const tempo_command::CommandConfig &commandConfig, AgentConfig &agentConfig);
}

#endif // CHORD_AGENT_AGENT_CONFIG_H