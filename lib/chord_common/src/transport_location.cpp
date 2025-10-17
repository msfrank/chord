
#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>
#include <chord_common/transport_location.h>

chord_common::TransportLocation::TransportLocation()
    : m_type(TransportType::Invalid)
{
}

chord_common::TransportLocation::TransportLocation(TransportType type, const tempo_utils::Url &location)
    : m_type(type),
      m_location(location)
{
}

chord_common::TransportLocation::TransportLocation(const TransportLocation &other)
    : m_type(other.m_type),
      m_location(other.m_location)
{
}

bool
chord_common::TransportLocation::isValid() const
{
    return m_type != TransportType::Invalid;
}

chord_common::TransportType
chord_common::TransportLocation::getType() const
{
    return m_type;
}

std::filesystem::path
chord_common::TransportLocation::getUnixPath(const std::filesystem::path &relativeBase) const
{
    if (m_type != TransportType::Unix)
        return {};
    std::filesystem::path baseDir;
    if (m_location.toAuthority().toString() == ".") {
        baseDir = relativeBase.empty()? std::filesystem::current_path() : relativeBase;
    } else {
        baseDir = std::filesystem::current_path().root_path();
    }
    return m_location.toFilesystemPath(baseDir);
}

std::string
chord_common::TransportLocation::getTcp4Address() const
{
    if (m_type != TransportType::Tcp4)
        return {};
    return m_location.getHost();
}

bool
chord_common::TransportLocation::hasTcp4Port() const
{
    if (m_type != TransportType::Tcp4)
        return false;
    return !m_location.getPort().empty();
}

tu_uint16
chord_common::TransportLocation::getTcp4Port() const
{
    if (m_type != TransportType::Tcp4)
        return 0;
    auto port = m_location.getPort();
    if (port.empty())
        return 0;
    tu_uint32 portNumberU32;
    if (!absl::SimpleAtoi(port, &portNumberU32))
        return 0;
    if (std::numeric_limits<tu_uint16>::max() < portNumberU32)
        return 0;
    return static_cast<tu_uint16>(portNumberU32);
}

tempo_utils::Url
chord_common::TransportLocation::toUrl() const
{
    return m_location;
}

std::string
chord_common::TransportLocation::toString() const
{
    return m_location.toString();
}

chord_common::TransportLocation
chord_common::TransportLocation::forUnix(const std::filesystem::path &path)
{
    std::filesystem::path generic(path, std::filesystem::path::format::generic_format);
    if (path.is_absolute()) {
        auto url = tempo_utils::Url::fromString(
            absl::StrCat("chord+unix://", generic.string()));
        return TransportLocation(TransportType::Unix, url);
    } else {
        auto url = tempo_utils::Url::fromString(
            absl::StrCat("chord+unix://./", generic.string()));
        return TransportLocation(TransportType::Unix, url);
    }
}

chord_common::TransportLocation
chord_common::TransportLocation::forTcp4(std::string_view address, tu_uint16 port)
{
    std::string tcp4;
    if (port > 0) {
        tcp4 = absl::StrCat(address, ":", port);
    } else {
        tcp4 = address;
    }
    auto url = tempo_utils::Url::fromString(
        absl::StrCat("chord+tcp4://", tcp4));
    return TransportLocation(TransportType::Tcp4, url);
}

static bool validate_unix(const tempo_utils::Url &url)
{
    if (url.hasAuthority()) {
        auto authority = url.toAuthority();
        if (authority.toString() != ".")
            return false;
    }
    return url.hasPath();
}

static bool validate_tcp4(const tempo_utils::Url &url)
{
    if (!url.hasAuthority())
        return false;
    auto authority = url.toAuthority();
    return authority.hasHost();
}

chord_common::TransportLocation
chord_common::TransportLocation::fromUrl(const tempo_utils::Url &url)
{
    if (!url.hasScheme())
        return {};
    auto scheme = url.getScheme();

    if (scheme == "chord+unix" && validate_unix(url))
        return TransportLocation(TransportType::Unix, url);
    if (scheme == "chord+tcp4" && validate_tcp4(url))
        return TransportLocation(TransportType::Tcp4, url);

    return {};
}

chord_common::TransportLocation
chord_common::TransportLocation::fromString(std::string_view s)
{
    auto url = tempo_utils::Url::fromString(s);
    if (!url.isValid())
        return {};
    return fromUrl(url);
}