
#include <chord_sandbox/local_endpoint_signer.h>
#include <tempo_security/x509_certificate_signing_request.h>

chord_sandbox::LocalEndpointSigner::LocalEndpointSigner(const tempo_security::CertificateKeyPair &localCAKeypair)
    : m_localCAKeypair(localCAKeypair)
{
    TU_ASSERT (m_localCAKeypair.isValid());
}

tempo_utils::Result<std::string>
chord_sandbox::LocalEndpointSigner::signEndpoint(
    const tempo_utils::Url &endpointUrl,
    std::string_view pemRequestBytes,
    absl::Duration requestedValidityPeriod)
{
    auto validityInSeconds = ToChronoSeconds(requestedValidityPeriod);
    std::string pemCertificateBytes;
    TU_ASSIGN_OR_RETURN (pemCertificateBytes, tempo_security::generate_certificate_from_csr(
        pemRequestBytes, m_localCAKeypair, 1, validityInSeconds));

    return pemCertificateBytes;
}
