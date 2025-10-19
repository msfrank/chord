#ifndef CHORD_TOOLING_SECURITY_CONFIG_H
#define CHORD_TOOLING_SECURITY_CONFIG_H

#include <tempo_config/config_types.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_utils/result.h>

namespace chord_tooling {

    struct SecurityConfig {
        std::filesystem::path pemRootCABundleFile;
        std::filesystem::path pemSigningCertificateFile;
        std::filesystem::path pemSigningPrivateKeyFile;

        tempo_utils::Result<tempo_security::CertificateKeyPair> getSigningKeypair() const;
    };
}
#endif // CHORD_TOOLING_SECURITY_CONFIG_H