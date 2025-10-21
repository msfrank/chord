
#include <chord_tooling/signer_store.h>
#include <chord_tooling/tooling_conversions.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/container_conversions.h>
#include <tempo_config/parse_config.h>

tempo_utils::Result<tempo_security::CertificateKeyPair>
chord_tooling::SignerEntry::getSigningKeyPair() const
{
    return tempo_security::CertificateKeyPair::load(pemSigningPrivateKeyFile, pemSigningCertificateFile);
}

chord_tooling::SignerStore::SignerStore(
    const absl::flat_hash_map<std::string,std::shared_ptr<const SignerEntry>> &signerEntries)
    : m_signerEntries(signerEntries)
{
}

bool
chord_tooling::SignerStore::hasSigner(std::string_view signerName) const
{
    return m_signerEntries.contains(signerName);
}

std::shared_ptr<const chord_tooling::SignerEntry>
chord_tooling::SignerStore::getSigner(std::string_view signerName) const
{
    auto entry = m_signerEntries.find(signerName);
    if (entry != m_signerEntries.cend())
        return entry->second;
    return {};
}

absl::flat_hash_map<std::string,std::shared_ptr<const chord_tooling::SignerEntry>>::const_iterator
chord_tooling::SignerStore::signersBegin() const
{
    return m_signerEntries.cbegin();
}

absl::flat_hash_map<std::string,std::shared_ptr<const chord_tooling::SignerEntry>>::const_iterator
chord_tooling::SignerStore::signersEnd() const
{
    return m_signerEntries.cend();
}

int
chord_tooling::SignerStore::numSigners() const
{
    return m_signerEntries.size();
}

tempo_utils::Status
chord_tooling::SignerStore::putSigner(std::string_view signerName, std::shared_ptr<const SignerEntry> signerEntry)
{
    m_signerEntries[signerName] = std::move(signerEntry);
    return {};
}

tempo_utils::Status
chord_tooling::SignerStore::removeSigner(std::string_view signerName)
{
    m_signerEntries.erase(signerName);
    return {};
}
