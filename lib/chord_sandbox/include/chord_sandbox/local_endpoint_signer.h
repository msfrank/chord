#ifndef CHORD_SANDBOX_LOCAL_ENDPOINT_SIGNER_H
#define CHORD_SANDBOX_LOCAL_ENDPOINT_SIGNER_H
#include <tempo_security/certificate_key_pair.h>

#include "abstract_endpoint_signer.h"

namespace chord_sandbox {

    class LocalEndpointSigner : public AbstractEndpointSigner {
    public:
        explicit LocalEndpointSigner(const tempo_security::CertificateKeyPair &localCAKeypair);

        tempo_utils::Result<std::string> signEndpoint(
            const tempo_utils::Url &endpointUrl,
            std::string_view pemRequestBytes,
            absl::Duration requestedValidityPeriod) override;

    private:
        tempo_security::CertificateKeyPair m_localCAKeypair;
    };
}

#endif // CHORD_SANDBOX_LOCAL_ENDPOINT_SIGNER_H
