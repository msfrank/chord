#ifndef CHORD_COMMON_TRANSPORT_LOCATION_H
#define CHORD_COMMON_TRANSPORT_LOCATION_H

#include <filesystem>

#include <tempo_utils/integer_types.h>
#include <tempo_utils/url.h>

namespace chord_common {

    constexpr const char *kChordUnixScheme = "chord+unix";
    constexpr const char *kChordTcp4Scheme = "chord+tcp4";

    enum class TransportType {
        Invalid,
        Unix,
        Tcp4,
    };

    TransportType parse_transport_scheme(std::string_view s);
    const char *transport_scheme_to_string(TransportType type);

    class TransportLocation {
    public:
        TransportLocation();
        TransportLocation(const TransportLocation &other);

        bool isValid() const;

        TransportType getType() const;
        std::filesystem::path getUnixPath() const;
        std::string getTcp4Address() const;
        bool hasTcp4Port() const;
        tu_uint16 getTcp4Port() const;
        std::string getServerName() const;

        std::string toGrpcTarget() const;
        std::string toString() const;

        bool operator==(const TransportLocation &other) const;
        bool operator!=(const TransportLocation &other) const;

        static TransportLocation forUnix(
            const std::string &serverName,
            const std::filesystem::path &unixPath);
        static TransportLocation forTcp4(
            const std::string &serverName,
            const std::string &tcpAddress,
            tu_uint16 tcpPort = 0);

        static TransportLocation fromString(std::string_view s);

        template <typename H>
        friend H AbslHashValue(H h, const TransportLocation &location) {
            if (!location.isValid())
                return H::combine(std::move(h), TransportType::Invalid);
            auto &priv = location.m_priv;
            return H::combine(std::move(h), priv->serverName, priv->endpointTarget, priv->type);
        }

    private:
        struct Priv {
            TransportType type;
            std::string endpointTarget;
            std::string serverName;
        };
        std::shared_ptr<Priv> m_priv;

        TransportLocation(
            TransportType type,
            const std::string &endpointTarget,
            const std::string &serverName);
    };
}

#endif // CHORD_COMMON_TRANSPORT_LOCATION_H