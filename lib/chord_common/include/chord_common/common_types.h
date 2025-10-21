#ifndef CHORD_COMMON_PROTOCOL_TYPES_H
#define CHORD_COMMON_PROTOCOL_TYPES_H

#include <tempo_utils/url.h>

namespace chord_common {

    enum class SignerType {
        Invalid,
        Local,
    };

    enum class PortType {
        Invalid,
        OneShot,
        Streaming,
    };

    enum class PortDirection {
        Invalid,
        Client,
        Server,
        BiDirectional,
    };

    class RequestedPort {
    public:
        RequestedPort();
        RequestedPort(const tempo_utils::Url &portUrl, PortType portType, PortDirection portDirection);
        RequestedPort(const RequestedPort &other);

        bool isValid() const;

        tempo_utils::Url getUrl() const;
        PortType getType() const;
        PortDirection getDirection() const;

        bool operator==(const RequestedPort &other) const;
        bool operator!=(const RequestedPort &other) const;

        template <typename H>
        friend H AbslHashValue(H h, const RequestedPort &requestedPort) {
            return H::combine(std::move(h),
                requestedPort.m_url,
                static_cast<tu_uint8>(requestedPort.m_type),
                static_cast<tu_uint8>(requestedPort.m_direction));
        }

    private:
        tempo_utils::Url m_url;
        PortType m_type;
        PortDirection m_direction;
    };
}

#endif // CHORD_COMMON_PROTOCOL_TYPES_H
