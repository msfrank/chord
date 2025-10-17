#ifndef CHORD_COMMON_TRANSPORT_LOCATION_H
#define CHORD_COMMON_TRANSPORT_LOCATION_H

#include <filesystem>

#include <tempo_utils/integer_types.h>
#include <tempo_utils/url.h>

namespace chord_common {

    enum class TransportType {
        Invalid,
        Unix,
        Tcp4,
    };

    class TransportLocation {
    public:
        TransportLocation();
        TransportLocation(const TransportLocation &other);

        bool isValid() const;

        TransportType getType() const;
        std::filesystem::path getUnixPath(const std::filesystem::path &relativeBase = {}) const;
        std::string getTcp4Address() const;
        bool hasTcp4Port() const;
        tu_uint16 getTcp4Port() const;

        tempo_utils::Url toUrl() const;
        std::string toString() const;

        static TransportLocation forUnix(const std::filesystem::path &path);
        static TransportLocation forTcp4(std::string_view address, tu_uint16 port = 0);
        static TransportLocation fromUrl(const tempo_utils::Url &url);
        static TransportLocation fromString(std::string_view s);

    private:
        TransportType m_type;
        tempo_utils::Url m_location;

        TransportLocation(TransportType type, const tempo_utils::Url &location);
    };
}

#endif // CHORD_COMMON_TRANSPORT_LOCATION_H