
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
    tempo_config::PathParser pemSigningCertificateFileParser;
    tempo_config::PathParser pemSigningPrivateKeyFileParser;

    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(m_pemRootCABundleFile,
        pemRootCABundleFileParser, m_securityMap, "pemRootCABundleFile"));

    std::filesystem::path pemSigningCertificateFile;
    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(pemSigningCertificateFile,
        pemSigningCertificateFileParser, m_securityMap, "pemSigningCertificateFile"));

    std::filesystem::path pemSigningPrivateKeyFile;
    TU_RETURN_IF_NOT_OK (tempo_config::parse_config(pemSigningPrivateKeyFile,
        pemSigningPrivateKeyFileParser, m_securityMap, "pemSigningPrivateKeyFile"));

    TU_ASSIGN_OR_RETURN (m_signingKeypair, tempo_security::CertificateKeyPair::load(
        pemSigningPrivateKeyFile, pemSigningCertificateFile));

    return {};
}

std::filesystem::path
chord_tooling::SecurityConfig::getPemRootCABundleFile() const
{
    return m_pemRootCABundleFile;
}

std::filesystem::path
chord_tooling::SecurityConfig::getPemSigningCertificateFile() const
{
    return m_signingKeypair.getPemCertificateFile();
}

std::filesystem::path
chord_tooling::SecurityConfig::getPemSigningPrivateKeyFile() const
{
    return m_signingKeypair.getPemPrivateKeyFile();
}