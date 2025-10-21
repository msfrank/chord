
#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>

#include <chord_common/transport_location.h>


chord_common::TransportType
chord_common::parse_transport_scheme(std::string_view s)
{
    if (s == std::string_view(kChordUnixScheme))
        return TransportType::Unix;
    if (s == std::string_view(kChordTcp4Scheme))
        return TransportType::Tcp4;
    return TransportType::Invalid;
}

const char *
chord_common::transport_scheme_to_string(TransportType type)
{
    switch (type) {
        case TransportType::Unix:
            return kChordUnixScheme;
        case TransportType::Tcp4:
            return kChordTcp4Scheme;
        default:
            return nullptr;
    }
}

chord_common::TransportLocation::TransportLocation()
{
}

chord_common::TransportLocation::TransportLocation(
    TransportType type,
    const std::string &endpointTarget,
    const std::string &serverName)
{
    m_priv = std::make_shared<Priv>(type, endpointTarget, serverName);
}

chord_common::TransportLocation::TransportLocation(const TransportLocation &other)
    : m_priv(other.m_priv)
{
}

bool
chord_common::TransportLocation::isValid() const
{
    return m_priv != nullptr;
}

chord_common::TransportType
chord_common::TransportLocation::getType() const
{
    if (m_priv == nullptr)
        return TransportType::Invalid;
    return m_priv->type;
}

std::filesystem::path
chord_common::TransportLocation::getUnixPath() const
{
    if (!m_priv || m_priv->type != TransportType::Unix)
        return {};
    std::filesystem::path unixPath(m_priv->endpointTarget);
    return unixPath;
}

std::string
chord_common::TransportLocation::getTcp4Address() const
{
    if (!m_priv || m_priv->type != TransportType::Tcp4)
        return {};
    auto sep = m_priv->endpointTarget.find(':');
    if (sep != std::string::npos)
        return m_priv->endpointTarget.substr(0, sep);
    return m_priv->endpointTarget;
}

bool
chord_common::TransportLocation::hasTcp4Port() const
{
    if (!m_priv || m_priv->type != TransportType::Tcp4)
        return {};
    auto sep = m_priv->endpointTarget.find(':');
    return sep != std::string::npos;
}

inline bool string_to_port_u16(std::string_view sv, tu_uint16 *port = nullptr)
{
    if (sv.empty())
        return false;
    tu_uint32 portNumberU32;
    if (!absl::SimpleAtoi(sv, &portNumberU32))
        return false;
    if (std::numeric_limits<tu_uint16>::max() < portNumberU32)
        return false;
    if (port != nullptr) {
        *port = static_cast<tu_uint16>(portNumberU32);
    }
    return true;
}

tu_uint16
chord_common::TransportLocation::getTcp4Port() const
{
    if (!m_priv || m_priv->type != TransportType::Tcp4)
        return {};
    auto sep = m_priv->endpointTarget.find(':');
    if (sep == std::string::npos)
        return 0;
    auto port = m_priv->endpointTarget.substr(sep);
    tu_uint16 portNumber;
    if (!string_to_port_u16(port, &portNumber))
        return 0;
    return portNumber;
}

std::string
chord_common::TransportLocation::getServerName() const
{
    if (m_priv == nullptr)
        return {};
    return m_priv->serverName;
}

std::string
chord_common::TransportLocation::toGrpcTarget() const
{
    if (m_priv == nullptr)
        return {};
    switch (m_priv->type) {
        case TransportType::Unix: {
            auto path = getUnixPath();
            //return absl::StrCat("unix://", std::filesystem::absolute(path).string());
            return absl::StrCat("unix:", std::filesystem::absolute(path).string());
        }
        case TransportType::Tcp4: {
            std::string target;
            auto address = getTcp4Address();
            if (std::isdigit(address.front())) {
                target = absl::StrCat("ipv4:", address);
            } else {
                target = absl::StrCat("dns:///", address);
            }
            auto portNumber = getTcp4Port();
            if (portNumber > 0) {
                absl::StrAppend(&target, ":", portNumber);
            }
            return target;
        }
        default:
            return {};
    }
}

std::string
chord_common::TransportLocation::toString() const
{
    if (m_priv == nullptr)
        return {};
    return absl::StrCat(transport_scheme_to_string(m_priv->type),
        ":",
        m_priv->serverName,
        ":",
        m_priv->endpointTarget);
}

chord_common::TransportLocation
chord_common::TransportLocation::forUnix(
    const std::string &serverName,
    const std::filesystem::path &unixPath)
{
    std::filesystem::path endpointTarget(unixPath, std::filesystem::path::format::generic_format);
    if (!endpointTarget.is_absolute()) {
        endpointTarget = std::filesystem::absolute(endpointTarget);
    }
    return TransportLocation(TransportType::Unix, endpointTarget.string(), serverName);
}

chord_common::TransportLocation
chord_common::TransportLocation::forTcp4(
    const std::string &serverName,
    const std::string &tcpAddress,
    tu_uint16 tcpPort)
{
    auto sep = tcpAddress.find(':');
    auto address = sep != std::string::npos? tcpAddress.substr(0, sep) : tcpAddress;
    auto port = sep != std::string::npos? tcpAddress.substr(sep) : std::string{};

    if (!port.empty() && !string_to_port_u16(port))
        return {};

    std::string endpointTarget;
    if (tcpPort > 0) {
        endpointTarget = absl::StrCat(address, ":", tcpPort);
    } else if (!port.empty()) {
        endpointTarget = absl::StrCat(address, ":", port);
    } else {
        endpointTarget = address;
    }
    return TransportLocation(TransportType::Tcp4, endpointTarget, serverName);
}

chord_common::TransportLocation
chord_common::TransportLocation::fromString(std::string_view s)
{
    std::vector<std::string> parts = absl::StrSplit(s, absl::MaxSplits(':', 3));
    if (parts.size() < 3)
        return {};
    auto &scheme = parts.at(0);
    auto type = parse_transport_scheme(scheme);
    if (type == TransportType::Invalid)
        return {};
    auto &serverName = parts.at(1);
    auto &endpointTarget = parts.at(2);
    return TransportLocation(type, endpointTarget, serverName);
}