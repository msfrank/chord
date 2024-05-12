
#include <chord_protocol/protocol_types.h>

chord_protocol::RequestedPort::RequestedPort()
    : m_type(PortType::Invalid),
      m_direction(PortDirection::Invalid)
{
}

chord_protocol::RequestedPort::RequestedPort(
    const tempo_utils::Url &portUrl,
    PortType portType,
    PortDirection portDirection)
    : m_url(portUrl),
      m_type(portType),
      m_direction(portDirection)
{
    TU_ASSERT (m_url.isValid());
    TU_ASSERT (m_type != PortType::Invalid);
    TU_ASSERT (m_direction != PortDirection::Invalid);
}

chord_protocol::RequestedPort::RequestedPort(const RequestedPort &other)
    : m_url(other.m_url),
      m_type(other.m_type),
      m_direction(other.m_direction)
{
}

tempo_utils::Url
chord_protocol::RequestedPort::getUrl() const
{
    return m_url;
}

chord_protocol::PortType
chord_protocol::RequestedPort::getType() const
{
    return m_type;
}

chord_protocol::PortDirection
chord_protocol::RequestedPort::getDirection() const
{
    return m_direction;
}

bool
chord_protocol::RequestedPort::operator==(const RequestedPort &other) const
{
    return m_url == other.m_url && m_type == other.m_type && m_direction == other.m_direction;
}

bool
chord_protocol::RequestedPort::operator!=(const RequestedPort &other) const
{
    return !(*this == other);
}
