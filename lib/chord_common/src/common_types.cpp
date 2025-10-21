
#include <chord_common/common_types.h>

chord_common::RequestedPort::RequestedPort()
    : m_type(PortType::Invalid),
      m_direction(PortDirection::Invalid)
{
}

chord_common::RequestedPort::RequestedPort(
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

chord_common::RequestedPort::RequestedPort(const RequestedPort &other)
    : m_url(other.m_url),
      m_type(other.m_type),
      m_direction(other.m_direction)
{
}

tempo_utils::Url
chord_common::RequestedPort::getUrl() const
{
    return m_url;
}

chord_common::PortType
chord_common::RequestedPort::getType() const
{
    return m_type;
}

chord_common::PortDirection
chord_common::RequestedPort::getDirection() const
{
    return m_direction;
}

bool
chord_common::RequestedPort::operator==(const RequestedPort &other) const
{
    return m_url == other.m_url && m_type == other.m_type && m_direction == other.m_direction;
}

bool
chord_common::RequestedPort::operator!=(const RequestedPort &other) const
{
    return !(*this == other);
}
