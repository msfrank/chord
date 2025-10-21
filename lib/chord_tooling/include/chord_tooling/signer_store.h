#ifndef CHORD_TOOLING_SIGNER_STORE_H
#define CHORD_TOOLING_SIGNER_STORE_H

#include <tempo_config/config_types.h>
#include <tempo_security/certificate_key_pair.h>
#include <tempo_utils/status.h>

namespace chord_tooling {

    struct SignerEntry {
        std::vector<std::filesystem::path> packageCacheDirectories;
        std::filesystem::path pemRootCABundleFile;
        std::filesystem::path pemSigningCertificateFile;
        std::filesystem::path pemSigningPrivateKeyFile;
        absl::Duration validityPeriod;

        tempo_utils::Result<tempo_security::CertificateKeyPair> getSigningKeyPair() const;
    };

    class SignerStore {
    public:
        explicit SignerStore(
            const absl::flat_hash_map<std::string,std::shared_ptr<const SignerEntry>> &signerEntries = {});

        bool hasSigner(std::string_view signerName) const;
        std::shared_ptr<const SignerEntry> getSigner(std::string_view signerName) const;
        absl::flat_hash_map<std::string,std::shared_ptr<const SignerEntry>>::const_iterator signersBegin() const;
        absl::flat_hash_map<std::string,std::shared_ptr<const SignerEntry>>::const_iterator signersEnd() const;
        int numSigners() const;

        tempo_utils::Status putSigner(std::string_view signerName, std::shared_ptr<const SignerEntry> signerEntry);
        tempo_utils::Status removeSigner(std::string_view signerName);

    private:
        absl::flat_hash_map<std::string,std::shared_ptr<const SignerEntry>> m_signerEntries;
    };
}

#endif // CHORD_TOOLING_SIGNER_STORE_H