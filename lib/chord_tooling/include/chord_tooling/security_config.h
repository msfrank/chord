#ifndef CHORD_TOOLING_SECURITY_CONFIG_H
#define CHORD_TOOLING_SECURITY_CONFIG_H

#include <tempo_config/config_types.h>
#include <tempo_utils/status.h>

namespace chord_tooling {

    class SecurityConfig {

    public:
        explicit SecurityConfig(const tempo_config::ConfigMap &securityMap);

        tempo_utils::Status configure();

        std::filesystem::path getPemRootCABundleFile() const;

    private:
        tempo_config::ConfigMap m_securityMap;

        std::filesystem::path m_pemRootCABundleFile;
    };
}
#endif // CHORD_TOOLING_SECURITY_CONFIG_H