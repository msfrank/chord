
#include <chord_mesh/rep_protocol.h>

tempo_utils::Status
chord_mesh::RepStreamImpl::validate(
    std::string_view protocolName,
    std::shared_ptr<tempo_security::X509Certificate> certificate)
{
    return {};
}

void
chord_mesh::RepStreamImpl::error(const tempo_utils::Status &status)
{
}

void
chord_mesh::RepStreamImpl::cleanup()
{
}