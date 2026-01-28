#ifndef CHORD_TOOLING_SECURITY_CONFIG_H
#define CHORD_TOOLING_SECURITY_CONFIG_H

#include <tempo_config/config_types.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_utils/result.h>

namespace chord_tooling {

    class SecurityConfig {
    public:
        explicit SecurityConfig(const tempo_config::ConfigMap &securityMap);

        tempo_utils::Status configure();

        std::filesystem::path getPemRootCABundleFile() const;
        std::filesystem::path getPemSigningCertificateFile() const;
        std::filesystem::path getPemSigningPrivateKeyFile() const;

        tempo_security::CertificateKeyPair getSigningKeypair() const;

    private:
        tempo_config::ConfigMap m_securityMap;

        std::filesystem::path m_pemRootCABundleFile;
        tempo_security::CertificateKeyPair m_signingKeypair;
    };
}
#endif // CHORD_TOOLING_SECURITY_CONFIG_H