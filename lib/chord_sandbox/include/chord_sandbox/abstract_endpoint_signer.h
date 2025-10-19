#ifndef CHORD_SANDBOX_ABSTRACT_ENDPOINT_SIGNER_H
#define CHORD_SANDBOX_ABSTRACT_ENDPOINT_SIGNER_H

#include <tempo_utils/result.h>
#include <tempo_utils/url.h>

namespace chord_sandbox {

    class AbstractEndpointSigner {
    public:
        virtual ~AbstractEndpointSigner() = default;

        virtual tempo_utils::Result<std::string> signEndpoint(
            const tempo_utils::Url &endpointUrl,
            std::string_view pemRequestBytes,
            absl::Duration requestedValidityPeriod) = 0;
    };
}

#endif // CHORD_SANDBOX_ABSTRACT_ENDPOINT_SIGNER_H