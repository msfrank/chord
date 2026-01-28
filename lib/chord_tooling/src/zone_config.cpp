
#include <chord_tooling/zone_config.h>
#include <tempo_config/config_builder.h>
#include <tempo_config/config_utils.h>
#include <tempo_config/merge_map.h>

chord_tooling::ZoneConfig::ZoneConfig(
    const Zone &zone,
    std::shared_ptr<zuri_tooling::EnvironmentConfig> environmentConfig,
    std::shared_ptr<zuri_tooling::CoreConfig> coreConfig,
    const tempo_config::ConfigMap &configMap)
    : m_zone(zone),
      m_environmentConfig(std::move(environmentConfig)),
      m_coreConfig(std::move(coreConfig)),
      m_configMap(configMap)
{
    TU_ASSERT (m_zone.isValid());
    TU_ASSERT (m_environmentConfig != nullptr);
    TU_ASSERT (m_coreConfig != nullptr);
}

static tempo_utils::Result<tempo_config::ConfigMap>
load_zone_override_config()
{
    const auto *value = std::getenv(chord_tooling::kEnvOverrideZoneConfigName);
    if (value == nullptr)
        return tempo_config::ConfigMap{};
    tempo_config::ConfigNode overrideNode;
    TU_ASSIGN_OR_RETURN (overrideNode, tempo_config::read_config_string(value));
    TU_LOG_V << "parsed zone override config: " << overrideNode.toString();

    if (overrideNode.getNodeType() != tempo_config::ConfigNodeType::kMap)
        return tempo_config::ConfigStatus::forCondition(tempo_config::ConfigCondition::kWrongType,
            "invalid type for environment variable {}; expected a map",
            chord_tooling::kEnvOverrideZoneConfigName);

    return tempo_config::startMap()
        .put("chord", tempo_config::startMap()
            .put("zone", overrideNode).buildNode())
        .buildMap();
}

static tempo_utils::Result<tempo_config::ConfigMap>
read_zone_config(
    const chord_tooling::Zone &zone,
    const tempo_config::ConfigMap &root)
{
    // build the default config
    tempo_config::ConfigMap defaultConfig;
    if (root.mapContains("chord")) {
        auto chordMap = root.mapAt("chord").toMap();
        if (chordMap.mapContains("defaults")) {
            defaultConfig = chordMap.mapAt("defaults").toMap();
        }
    }

    // parse the config/ directory config
    auto configDirectory = zone.getConfigDirectory();
    tempo_config::ConfigMap directoryConfig;
    if (std::filesystem::exists(configDirectory)) {
        TU_ASSIGN_OR_RETURN (directoryConfig, tempo_config::read_config_tree_directory(
            configDirectory, ".config"));
    }

    // parse the zone config file
    tempo_config::ConfigMap zoneMap;
    TU_ASSIGN_OR_RETURN (zoneMap, tempo_config::read_config_map_file(zone.getZoneConfigFile()));
    auto zoneConfig = tempo_config::startMap()
        .put("chord", tempo_config::startMap()
            .put("zone", zoneMap).buildNode())
        .buildMap();

    // load override config if present
    tempo_config::ConfigMap overrideConfig;
    TU_ASSIGN_OR_RETURN (overrideConfig, load_zone_override_config());

    // return the merged config
    return tempo_config::merge_map(defaultConfig,
        tempo_config::merge_map(zoneConfig,
            tempo_config::merge_map(zoneConfig,
                overrideConfig)));
}

tempo_utils::Status
chord_tooling::ZoneConfig::configure()
{
    auto chordMap = m_configMap.mapAt("chord").toMap();
    if (chordMap.isNil())
        return tempo_config::ConfigStatus::forCondition(tempo_config::ConfigCondition::kMissingValue,
            "missing required config 'chord'");

    auto zoneMap = chordMap.mapAt("zone").toMap();
    if (chordMap.isNil())
        return tempo_config::ConfigStatus::forCondition(tempo_config::ConfigCondition::kMissingValue,
            "missing required key 'chord.zone'");

    // parse chord.zone.security
    auto securityMap = zoneMap.mapAt("security").toMap();
    if (securityMap.getNodeType() == tempo_config::ConfigNodeType::kMap) {
        m_securityConfig = std::make_shared<SecurityConfig>(securityMap);
        TU_RETURN_IF_NOT_OK (m_securityConfig->configure());
    }

    return {};
}

tempo_utils::Result<std::shared_ptr<chord_tooling::ZoneConfig>>
chord_tooling::ZoneConfig::load(
    const Zone &zone,
    std::shared_ptr<zuri_tooling::CoreConfig> coreConfig)
{
    auto environmentDirectory = zone.getEnvironmentDirectory();
    zuri_tooling::Environment environment;
    TU_ASSIGN_OR_RETURN (environment, zuri_tooling::Environment::open(environmentDirectory));
    std::shared_ptr<zuri_tooling::EnvironmentConfig> environmentConfig;
    TU_ASSIGN_OR_RETURN(environmentConfig, zuri_tooling::EnvironmentConfig::load(environment, coreConfig));

    tempo_config::ConfigMap configMap;
    TU_ASSIGN_OR_RETURN (configMap, read_zone_config(zone, coreConfig->getRoot()));

    auto projectConfig = std::shared_ptr<ZoneConfig>(new ZoneConfig(
        zone, environmentConfig, coreConfig, configMap));
    TU_RETURN_IF_NOT_OK (projectConfig->configure());

    return projectConfig;
}

chord_tooling::Zone
chord_tooling::ZoneConfig::getZone() const
{
    return m_zone;
}

std::shared_ptr<zuri_tooling::EnvironmentConfig>
chord_tooling::ZoneConfig::getEnvironmentConfig() const
{
    return m_environmentConfig;
}

std::shared_ptr<zuri_tooling::CoreConfig>
chord_tooling::ZoneConfig::getCoreConfig() const
{
    return m_coreConfig;
}

std::shared_ptr<chord_tooling::SecurityConfig>
chord_tooling::ZoneConfig::getSecurityConfig() const
{
    return m_securityConfig;
}
