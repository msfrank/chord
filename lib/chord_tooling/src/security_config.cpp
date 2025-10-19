
#include <chord_tooling/security_config.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/parse_config.h>

tempo_utils::Result<tempo_security::CertificateKeyPair>
chord_tooling::SecurityConfig::getSigningKeypair() const
{
    return tempo_security::CertificateKeyPair::load(
        pemSigningPrivateKeyFile, pemSigningCertificateFile);
}