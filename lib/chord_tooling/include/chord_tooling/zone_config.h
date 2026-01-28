#ifndef CHORD_TOOLING_ZONE_CONFIG_H
#define CHORD_TOOLING_ZONE_CONFIG_H

#include <tempo_config/config_types.h>
#include <tempo_utils/result.h>
#include <zuri_tooling/core_config.h>
#include <zuri_tooling/environment_config.h>

#include "security_config.h"
#include "zone.h"

namespace chord_tooling {

    /**
     *
     */
    constexpr const char * const kEnvOverrideZoneConfigName = "CHORD_OVERRIDE_ZONE_CONFIG";

    /**
     *
     */
    class ZoneConfig {

    public:
        static tempo_utils::Result<std::shared_ptr<ZoneConfig>> load(
            const Zone &zone,
            std::shared_ptr<zuri_tooling::CoreConfig> coreConfig);

        Zone getZone() const;
        std::shared_ptr<zuri_tooling::EnvironmentConfig> getEnvironmentConfig() const;
        std::shared_ptr<zuri_tooling::CoreConfig> getCoreConfig() const;

        std::shared_ptr<SecurityConfig> getSecurityConfig() const;

    private:
        Zone m_zone;
        std::shared_ptr<zuri_tooling::EnvironmentConfig> m_environmentConfig;
        std::shared_ptr<zuri_tooling::CoreConfig> m_coreConfig;
        tempo_config::ConfigMap m_configMap;

        std::shared_ptr<SecurityConfig> m_securityConfig;

        ZoneConfig(
            const Zone &zone,
            std::shared_ptr<zuri_tooling::EnvironmentConfig> environmentConfig,
            std::shared_ptr<zuri_tooling::CoreConfig> coreConfig,
            const tempo_config::ConfigMap &configMap);

        tempo_utils::Status configure();
    };
}

#endif // CHORD_TOOLING_ZONE_CONFIG_H