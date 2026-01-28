#ifndef CHORD_TOOLING_ZONE_H
#define CHORD_TOOLING_ZONE_H

#include <tempo_config/config_types.h>
#include <zuri_tooling/distribution.h>

namespace chord_tooling {

    /**
     *
     */
    constexpr const char * const kZoneConfigName = "zone.config";

    /**
     *
     */
    struct ZoneOpenOrCreateOptions {
        /**
         * if true then zone must not exist, and will be created.
         */
        bool exclusive = false;
        /**
         *
         */
        zuri_tooling::Distribution distribution = {};
        /**
         *
         */
        std::vector<std::filesystem::path> extraLibDirs = {};
        /**
         * map contents will be written to zone.config
         */
        tempo_config::ConfigMap zoneMap = {};
    };

    /**
     *
     */
    class Zone {
    public:
        Zone();
        Zone(const Zone &other);

        bool isValid() const;

        std::filesystem::path getZoneConfigFile() const;
        std::filesystem::path getZoneDirectory() const;
        std::filesystem::path getConfigDirectory() const;
        std::filesystem::path getEnvironmentDirectory() const;

        static tempo_utils::Result<Zone> openOrCreate(
            const std::filesystem::path &zoneDirectory,
            const ZoneOpenOrCreateOptions &options = {});
        static tempo_utils::Result<Zone> open(const std::filesystem::path &zoneDirectoryOrConfigFile);
        static tempo_utils::Result<Zone> find(const std::filesystem::path &searchStart = {});

    private:
        struct Priv {
            std::filesystem::path zoneConfigFile;
            std::filesystem::path zoneDirectory;
            std::filesystem::path configDirectory;
            std::filesystem::path environmentDirectory;
        };
        std::shared_ptr<Priv> m_priv;

        Zone(
            const std::filesystem::path &zoneConfigFile,
            const std::filesystem::path &zoneDirectory,
            const std::filesystem::path &configDirectory,
            const std::filesystem::path &environmentDirectory);
    };
}

#endif // CHORD_TOOLING_ZONE_H