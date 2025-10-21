#ifndef CHORD_COMMON_ABSTRACT_ENDPOINT_SIGNER_H
#define CHORD_COMMON_ABSTRACT_ENDPOINT_SIGNER_H

#include <tempo_utils/result.h>
#include <tempo_utils/url.h>

namespace chord_common {

    class AbstractCertificateSigner {
    public:
        virtual ~AbstractCertificateSigner() = default;

        virtual tempo_utils::Result<std::string> signSession(
            const tempo_utils::Url &sessionUrl,
            std::string_view pemRequestBytes,
            absl::Duration requestedValidityPeriod) = 0;

        virtual tempo_utils::Result<std::string> signEndpoint(
            const tempo_utils::Url &endpointUrl,
            std::string_view pemRequestBytes,
            absl::Duration requestedValidityPeriod) = 0;
    };
}

#endif // CHORD_COMMON_ABSTRACT_ENDPOINT_SIGNER_H