
#include <chord_common/common_types.h>
#include <chord_tooling/tooling_conversions.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/config_result.h>
#include <tempo_config/container_conversions.h>
#include <tempo_config/enum_conversions.h>
#include <tempo_config/parse_config.h>
#include <tempo_config/time_conversions.h>

tempo_utils::Status
chord_tooling::SecurityConfigParser::convertValue(
      const tempo_config::ConfigNode &node,
      SecurityConfig &securityConfig) const
{
    if (node.getNodeType() != tempo_config::ConfigNodeType::kMap)
        return tempo_config::ConfigStatus::forCondition(
            tempo_config::ConfigCondition::kWrongType, "security config must be a map");
    auto securityMap = node.toMap();

    tempo_config::PathParser pemRootCABundleFileParser;
    tempo_config::PathParser pemSigningCertificateFileParser;
    tempo_config::PathParser pemSigningPrivateKeyFileParser;

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(securityConfig.pemRootCABundleFile,
        pemRootCABundleFileParser, securityMap, "pemRootCABundleFile"));

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(securityConfig.pemSigningCertificateFile,
        pemSigningCertificateFileParser, securityMap, "pemSigningCertificateFile"));

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(securityConfig.pemSigningPrivateKeyFile,
        pemSigningPrivateKeyFileParser, securityMap, "pemSigningPrivateKeyFile"));

    return {};
}

tempo_utils::Status
chord_tooling::SignerEntryParser::parseLocalSigner(const tempo_config::ConfigMap &map, SignerEntry &signerEntry) const
{
    tempo_config::PathParser pemRootCABundleFileParser;
    tempo_config::PathParser pemSigningCertificateFileParser;
    tempo_config::PathParser pemSigningPrivateKeyFileParser;
    tempo_config::DurationParser validityPeriodParser(absl::Hours(4));

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(signerEntry.pemRootCABundleFile,
        pemRootCABundleFileParser, map, "pemRootCABundleFile"));

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(signerEntry.pemSigningCertificateFile,
        pemSigningCertificateFileParser, map, "pemSigningCertificateFile"));

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(signerEntry.pemSigningPrivateKeyFile,
        pemSigningPrivateKeyFileParser, map, "pemSigningPrivateKeyFile"));

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(signerEntry.validityPeriod,
        validityPeriodParser, map, "validityPeriod"));

    return {};
}

tempo_utils::Status
chord_tooling::SignerEntryParser::convertValue(const tempo_config::ConfigNode &node, SignerEntry &signerEntry) const
{
    if (node.getNodeType() != tempo_config::ConfigNodeType::kMap)
        return tempo_config::ConfigStatus::forCondition(
            tempo_config::ConfigCondition::kWrongType, "signer entry config must be a map");
    auto signerMap = node.toMap();

    // parse type
    tempo_config::EnumTParser<chord_common::SignerType> signerTypeParser({
        {"Local", chord_common::SignerType::Local},
    });

    chord_common::SignerType signerType;
    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(signerType,
        signerTypeParser, signerMap, "type"));
    switch (signerType) {
        case chord_common::SignerType::Local:
            TU_RETURN_IF_NOT_OK (parseLocalSigner(signerMap, signerEntry));
            break;
        default:
            return tempo_config::ConfigStatus::forCondition(
                tempo_config::ConfigCondition::kWrongType, "invalid signer type");
    }

    return {};
}

tempo_utils::Status
chord_tooling::SignerStoreParser::convertValue(const tempo_config::ConfigNode &node, SignerStore &signerStore) const
{
    if (node.getNodeType() != tempo_config::ConfigNodeType::kMap)
        return tempo_config::ConfigStatus::forCondition(
            tempo_config::ConfigCondition::kWrongType, "signer entry config must be a map");
    auto signersMap = node.toMap();

    SignerEntryParser signerEntryParser;
    tempo_config::SharedPtrConstTParser sharedConstSignerEntryParser(&signerEntryParser);

    absl::flat_hash_map<std::string,std::shared_ptr<const SignerEntry>> signerEntries;
    for (auto it = signersMap.mapBegin(); it != signersMap.mapEnd(); it++) {
        auto signerName = it->first;
        std::shared_ptr<const SignerEntry> signerEntry;
        TU_RETURN_IF_NOT_OK (tempo_config::parse_config(signerEntry, sharedConstSignerEntryParser, it->second));
        signerEntries[signerName] = std::move(signerEntry);
    }

    signerStore = SignerStore(signerEntries);
    return {};
}

tempo_utils::Status
chord_tooling::AgentEntryParser::convertValue(const tempo_config::ConfigNode &node, AgentEntry &agentEntry) const
{
    if (node.getNodeType() != tempo_config::ConfigNodeType::kMap)
        return tempo_config::ConfigStatus::forCondition(
            tempo_config::ConfigCondition::kWrongType, "agent entry config must be a map");
    auto agentMap = node.toMap();

    // parse type
    tempo_config::EnumTParser<chord_common::TransportType> transportLocationTypeParser({
        {"Unix", chord_common::TransportType::Unix},
        {"Tcp4", chord_common::TransportType::Tcp4},
    });
    tempo_config::StringParser certificateSignerParser(std::string{});
    tempo_config::PathParser packageCacheDirectoryParser;
    tempo_config::SeqTParser packageCacheDirectoriesParser(&packageCacheDirectoryParser, {});
    tempo_config::DurationParser idleTimeoutParser(absl::Duration{});
    tempo_config::DurationParser registrationTimeoutParser(absl::Seconds(15));

    chord_common::TransportType transportType;
    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(transportType,
        transportLocationTypeParser, agentMap, "agentTransport"));
    switch (transportType) {
        case chord_common::TransportType::Unix:
            TU_RETURN_IF_NOT_OK (parseUnixTransport(agentMap, agentEntry));
            break;
        case chord_common::TransportType::Tcp4:
            TU_RETURN_IF_NOT_OK (parseTcp4Transport(agentMap, agentEntry));
            break;
        default:
            return tempo_config::ConfigStatus::forCondition(
                tempo_config::ConfigCondition::kWrongType, "invalid agentTransport");
    }

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(agentEntry.packageCacheDirectories,
        packageCacheDirectoriesParser, agentMap, "packageCacheDirectories"));

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(agentEntry.certificateSigner,
        certificateSignerParser, agentMap, "certificateSigner"));

    return {};
}

tempo_utils::Status
chord_tooling::AgentEntryParser::parseUnixTransport(const tempo_config::ConfigMap &map, AgentEntry &agentEntry) const
{
    agentEntry.transportType = chord_common::TransportType::Unix;

    tempo_config::PathParser unixListenerPathParser;
    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(agentEntry.unixListenerPath,
        unixListenerPathParser, map, "unixListenerPath"));

    return {};
}

tempo_utils::Status
chord_tooling::AgentEntryParser::parseTcp4Transport(const tempo_config::ConfigMap &map, AgentEntry &agentEntry) const
{
    agentEntry.transportType = chord_common::TransportType::Tcp4;

    tempo_config::StringParser tcp4ListenerAddressParser;
    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(agentEntry.tcpListenerAddress,
        tcp4ListenerAddressParser, map, "tcp4ListenerAddress"));

    tempo_config::IntegerParser tcp4ListenerPortParser(0);
    int listenerPort;
    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(listenerPort,
        tcp4ListenerPortParser, map, "tcp4ListenerPort"));
    if (listenerPort < 0 || std::numeric_limits<tu_uint16>::max() < listenerPort)
        return tempo_config::ConfigStatus::forCondition(
            tempo_config::ConfigCondition::kWrongType, "tcp4ListenerPort is out of range");
    agentEntry.tcpListenerPort = static_cast<tu_uint16>(listenerPort);

    return {};
}

tempo_utils::Status
chord_tooling::AgentStoreParser::convertValue(const tempo_config::ConfigNode &node, AgentStore &agentStore) const
{
    if (node.getNodeType() != tempo_config::ConfigNodeType::kMap)
        return tempo_config::ConfigStatus::forCondition(
            tempo_config::ConfigCondition::kWrongType, "agent entry config must be a map");
    auto agentsMap = node.toMap();

    AgentEntryParser agentEntryParser;
    tempo_config::SharedPtrConstTParser sharedConstAgentEntryParser(&agentEntryParser);

    absl::flat_hash_map<std::string,std::shared_ptr<const AgentEntry>> agentEntries;
    for (auto it = agentsMap.mapBegin(); it != agentsMap.mapEnd(); it++) {
        auto agentName = it->first;
        std::shared_ptr<const AgentEntry> agentEntry;
        TU_RETURN_IF_NOT_OK (tempo_config::parse_config(agentEntry, sharedConstAgentEntryParser, it->second));
        agentEntries[agentName] = std::move(agentEntry);
    }

    agentStore = AgentStore(agentEntries);
    return {};
}
