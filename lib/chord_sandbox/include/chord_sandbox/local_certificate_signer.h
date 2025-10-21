#ifndef CHORD_SANDBOX_LOCAL_ENDPOINT_SIGNER_H
#define CHORD_SANDBOX_LOCAL_ENDPOINT_SIGNER_H

#include <chord_common/abstract_certificate_signer.h>
#include <tempo_security/certificate_key_pair.h>

namespace chord_sandbox {

    class LocalCertificateSigner : public chord_common::AbstractCertificateSigner {
    public:
        explicit LocalCertificateSigner(const tempo_security::CertificateKeyPair &localCAKeypair);

        tempo_utils::Result<std::string> signSession(
            const tempo_utils::Url &sessionUrl,
            std::string_view pemRequestBytes,
            absl::Duration requestedValidityPeriod) override;

        tempo_utils::Result<std::string> signEndpoint(
            const tempo_utils::Url &endpointUrl,
            std::string_view pemRequestBytes,
            absl::Duration requestedValidityPeriod) override;

    private:
        tempo_security::CertificateKeyPair m_localCAKeypair;
    };
}

#endif // CHORD_SANDBOX_LOCAL_ENDPOINT_SIGNER_H
