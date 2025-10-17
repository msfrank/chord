
#include <chord_tooling/tooling_conversions.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/config_result.h>
#include <tempo_config/container_conversions.h>
#include <tempo_config/enum_conversions.h>
#include <tempo_config/parse_config.h>
#include <tempo_config/time_conversions.h>

tempo_utils::Status
chord_tooling::AgentEntryParser::convertValue(const tempo_config::ConfigNode &node, AgentEntry &agentEntry) const
{
    if (node.getNodeType() != tempo_config::ConfigNodeType::kMap)
        return tempo_config::ConfigStatus::forCondition(
            tempo_config::ConfigCondition::kWrongType, "agent entry config must be a map");
    auto targetConfig = node.toMap();

    // parse type
    tempo_config::EnumTParser<chord_common::TransportType> transportLocationTypeParser({
        {"Unix", chord_common::TransportType::Unix},
        {"Tcp4", chord_common::TransportType::Tcp4},
    });
    tempo_config::PathParser pemCertificateFileParser;
    tempo_config::PathParser pemPrivateKeyFileParser;
    tempo_config::PathParser packageCacheDirectoryParser;
    tempo_config::SeqTParser packageCacheDirectoriesParser(&packageCacheDirectoryParser, {});
    tempo_config::DurationParser idleTimeoutParser(absl::Duration{});
    tempo_config::DurationParser registrationTimeoutParser(absl::Seconds(15));

    chord_common::TransportType transportType;
    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(transportType,
        transportLocationTypeParser, targetConfig, "agentTransport"));
    switch (transportType) {
        case chord_common::TransportType::Unix:
            TU_RETURN_IF_NOT_OK (parseUnixTransport(targetConfig, agentEntry));
            break;
        case chord_common::TransportType::Tcp4:
            TU_RETURN_IF_NOT_OK (parseTcp4Transport(targetConfig, agentEntry));
            break;
        default:
            return tempo_config::ConfigStatus::forCondition(
                tempo_config::ConfigCondition::kWrongType, "invalid agentTransport");
    }

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(agentEntry.packageCacheDirectories,
        packageCacheDirectoriesParser, targetConfig, "packageCacheDirectories"));

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(agentEntry.pemCertificateFile,
        pemCertificateFileParser, targetConfig, "pemCertificateFile"));

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(agentEntry.pemPrivateKeyFile,
        pemPrivateKeyFileParser, targetConfig, "pemPrivateKeyFile"));

    return {};
}

tempo_utils::Status
chord_tooling::AgentEntryParser::parseUnixTransport(const tempo_config::ConfigMap &map, AgentEntry &agentEntry) const
{
    tempo_config::StringParser tcp4ListenerAddressParser;
    tempo_config::IntegerParser tcp4ListenerPortParser(0);

    std::string tcp4ListenerAddress;
    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(tcp4ListenerAddress,
        tcp4ListenerAddressParser, map, "tcp4ListenerAddress"));

    int tcp4ListenerPort;
    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(tcp4ListenerPort,
        tcp4ListenerPortParser, map, "tcp4ListenerPort"));
    if (tcp4ListenerPort < 0 || std::numeric_limits<tu_uint16>::max() < tcp4ListenerPort)
        return tempo_config::ConfigStatus::forCondition(
            tempo_config::ConfigCondition::kWrongType, "tcp4ListenerPort is out of range");
    auto portNumber = static_cast<tu_uint16>(tcp4ListenerPort);

    agentEntry.agentLocation = chord_common::TransportLocation::forTcp4(tcp4ListenerAddress, portNumber);

    return {};}

tempo_utils::Status
chord_tooling::AgentEntryParser::parseTcp4Transport(const tempo_config::ConfigMap &map, AgentEntry &agentEntry) const
{
    tempo_config::PathParser unixListenerPathParser;

    std::filesystem::path unixListenerPath;
    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(unixListenerPath,
        unixListenerPathParser, map, "unixListenerPath"));

    agentEntry.agentLocation = chord_common::TransportLocation::forUnix(unixListenerPath);

    return {};
}
