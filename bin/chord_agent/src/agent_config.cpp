
#include <chord_agent/agent_config.h>
#include <tempo_config/base_conversions.h>

#include <chord_common/common_conversions.h>
#include <tempo_config/time_conversions.h>
#include <tempo_security/x509_certificate.h>

tempo_utils::Status
chord_agent::configure_agent(const tempo_command::CommandConfig &commandConfig, AgentConfig &agentConfig)
{
    auto cwd = std::filesystem::current_path();

    tempo_config::StringParser sessionNameParser;
    tempo_config::StringParser listenEndpointParser(std::string{});
    chord_common::TransportTypeParser listenTransportParser(chord_common::TransportType::Unix);
    tempo_config::PathParser runDirectoryParser(cwd);
    tempo_config::PathParser machineExecutableParser(std::filesystem::path(CHORD_MACHINE_EXECUTABLE));
    tempo_config::PathParser pemCertificateFileParser(kAgentCertificateFileName);
    tempo_config::PathParser pemPrivateKeyFileParser(kAgentPrivateKeyFileName);
    tempo_config::PathParser pemRootCABundleFileParser(kRootCABundleFileName);
    tempo_config::BooleanParser runInBackgroundParser(false);
    tempo_config::BooleanParser temporarySessionParser(false);
    tempo_config::DurationParser idleTimeoutParser(absl::Duration{});
    tempo_config::DurationParser registrationTimeoutParser(absl::Seconds(5));
    tempo_config::PathParser logFileParser(std::filesystem::path{});
    tempo_config::PathParser pidFileParser(std::filesystem::path{});
    tempo_config::PathParser endpointFileParser(std::filesystem::path{});

    // determine the session name
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.sessionName, sessionNameParser,
        commandConfig, "sessionName"));

    // determine the listen endpoint
    std::string listenEndpoint;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(listenEndpoint, listenEndpointParser,
        commandConfig, "listenEndpoint"));

    // determine the listen transport
    chord_common::TransportType listenTransport;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(listenTransport, listenTransportParser,
        commandConfig, "listenTransport"));

    // determine the process run directory
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.runDirectory, runDirectoryParser,
        commandConfig, "runDirectory"));

    // determine the local machine executable
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.machineExecutable, machineExecutableParser,
        commandConfig, "localMachineExecutable"));

    // determine the pem certificate file
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.pemCertificateFile, pemCertificateFileParser,
        commandConfig, "pemCertificateFile"));

    // determine the pem private key file
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.pemPrivateKeyFile, pemPrivateKeyFileParser,
        commandConfig, "pemPrivateKeyFile"));

    // determine the pem root CA bundle file
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.pemRootCABundleFile, pemRootCABundleFileParser,
        commandConfig, "pemRootCABundleFile"));

    // parse the run in background flag
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.runInBackground, runInBackgroundParser,
        commandConfig, "runInBackground"));

    // parse the temporary session flag
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.temporarySession, temporarySessionParser,
        commandConfig, "temporarySession"));

    // parse the idle timeout option
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.idleTimeout, idleTimeoutParser,
        commandConfig, "idleTimeout"));

    // parse the registration timeout option
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.registrationTimeout, registrationTimeoutParser,
        commandConfig, "registrationTimeout"));

    // determine the log file
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.logFile, logFileParser,
        commandConfig, "logFile"));

    // determine the pid file
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.pidFile, pidFileParser,
        commandConfig, "pidFile"));

    // determine the endpoint file
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentConfig.endpointFile, endpointFileParser,
        commandConfig, "endpointFile"));

    // if run directory was specified then adjust relative file paths

    if (!agentConfig.runDirectory.empty()) {
        const auto &runDirectory = agentConfig.runDirectory;

        if (agentConfig.pemCertificateFile.is_relative()) {
            agentConfig.pemCertificateFile = runDirectory / agentConfig.pemCertificateFile;
        }
        if (agentConfig.pemPrivateKeyFile.is_relative()) {
            agentConfig.pemPrivateKeyFile = runDirectory / agentConfig.pemPrivateKeyFile;
        }
        if (agentConfig.pemRootCABundleFile.is_relative()) {
            agentConfig.pemRootCABundleFile = runDirectory / agentConfig.pemRootCABundleFile;
        }
        if (agentConfig.logFile.is_relative()) {
            agentConfig.logFile = runDirectory / agentConfig.logFile;
        }
        if (agentConfig.pidFile.is_relative()) {
            agentConfig.pidFile = runDirectory / agentConfig.pidFile;
        }
        if (agentConfig.endpointFile.is_relative()) {
            agentConfig.endpointFile = runDirectory / agentConfig.endpointFile;
        }
    }

    // check for required files

    if (!std::filesystem::is_regular_file(agentConfig.pemRootCABundleFile))
        return tempo_command::CommandStatus::forCondition(
            tempo_command::CommandCondition::kInvalidConfiguration,
            "root CA file {} not found", agentConfig.pemRootCABundleFile.c_str());
    if (!std::filesystem::is_regular_file(agentConfig.pemCertificateFile))
        return tempo_command::CommandStatus::forCondition(
            tempo_command::CommandCondition::kInvalidConfiguration,
            "agent certificate {} not found", agentConfig.pemCertificateFile.c_str());
    if (!std::filesystem::is_regular_file(agentConfig.pemPrivateKeyFile))
        return tempo_command::CommandStatus::forCondition(
            tempo_command::CommandCondition::kInvalidConfiguration,
            "agent private key {} not found", agentConfig.pemPrivateKeyFile.c_str());

    // check for either endpoint or transport type

    if (listenEndpoint.empty() && listenTransport == chord_common::TransportType::Invalid)
        return tempo_command::CommandStatus::forCondition(
            tempo_command::CommandCondition::kInvalidConfiguration,
            "one of --listen-endpoint and --listen-transport must be specified");

    // get the endpoint server name from the certificate

    std::shared_ptr<tempo_security::X509Certificate> agentCert;
    TU_ASSIGN_OR_RETURN (agentCert, tempo_security::X509Certificate::readFile(agentConfig.pemCertificateFile));
    auto serverName = agentCert->getCommonName();

    // construct the listener target

    chord_common::TransportLocation listenLocation;
    if (listenEndpoint.empty()) {
        // case 1: endpoint was not specified so autoselect the location based on the transport type
        switch (listenTransport) {
            case chord_common::TransportType::Unix: {
                auto path = std::filesystem::absolute(agentConfig.runDirectory / "agent.sock");
                agentConfig.listenLocation = chord_common::TransportLocation::forUnix(serverName, path);
                break;
            }
            case chord_common::TransportType::Tcp4: {
                agentConfig.listenLocation = chord_common::TransportLocation::forTcp4(
                    serverName, "localhost");
                break;
            }
            default:
                return tempo_command::CommandStatus::forCondition(
                    tempo_command::CommandCondition::kCommandError,
                    "unknown --listen-transport type");
        }
    } else {
        auto maybeEndpoint = chord_common::TransportLocation::fromString(listenEndpoint);
        if (maybeEndpoint.isValid()) {
            // case 2: full endpoint was specified
            if (maybeEndpoint.getServerName() != serverName)
                return tempo_command::CommandStatus::forCondition(
                    tempo_command::CommandCondition::kCommandError,
                    "--listen-endpoint is a full uri and server name does not match the agent certificate");
            agentConfig.listenLocation = maybeEndpoint;
        } else {
            // case 3: partial endpoint was specified
            switch (listenTransport) {
                case chord_common::TransportType::Unix:
                    agentConfig.listenLocation = chord_common::TransportLocation::forUnix(
                        serverName, listenEndpoint);
                    break;
                case chord_common::TransportType::Tcp4:
                    agentConfig.listenLocation = chord_common::TransportLocation::forTcp4(
                        serverName, listenEndpoint);
                    break;
                default:
                    return tempo_command::CommandStatus::forCondition(
                        tempo_command::CommandCondition::kCommandError,
                        "--listen-endpoint is not a full uri and --listen-transport is not specified");
            }
        }
    }

    return {};
}
