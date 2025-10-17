
#include <chord_tooling/security_config.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/parse_config.h>

chord_tooling::SecurityConfig::SecurityConfig(const tempo_config::ConfigMap &securityMap)
    : m_securityMap(securityMap)
{
}

tempo_utils::Status
chord_tooling::SecurityConfig::configure()
{
    tempo_config::PathParser pemRootCABundleFileParser;

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(m_pemRootCABundleFile,
        pemRootCABundleFileParser, m_securityMap, "pemRootCABundleFile"));

    return {};
}

std::filesystem::path
chord_tooling::SecurityConfig::getPemRootCABundleFile() const
{
    return m_pemRootCABundleFile;
}