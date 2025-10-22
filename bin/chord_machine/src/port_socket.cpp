
#include <chord_machine/port_socket.h>
#include <tempo_utils/memory_bytes.h>

chord_machine::PortSocket::PortSocket(std::shared_ptr<lyric_runtime::DuplexPort> port)
    : m_port(port),
      m_writer(nullptr)
{
    TU_ASSERT (m_port != nullptr);
}

bool
chord_machine::PortSocket::isAttached()
{
    return m_writer != nullptr;
}

tempo_utils::Status
chord_machine::PortSocket::attach(chord_common::AbstractProtocolWriter *writer)
{
    m_writer = writer;
    auto status = m_port->attach(this);
    if (status.notOk())
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, status.getMessage());
    return tempo_utils::GenericStatus::ok();
}

tempo_utils::Status
chord_machine::PortSocket::send(std::string_view message)
{
    if (m_writer == nullptr)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "port is not available");
    TU_LOG_INFO << "port " << m_port->getUrl() << " sends message: " << std::string(message);
    return m_writer->write(message);
}

tempo_utils::Status
chord_machine::PortSocket::handle(std::string_view message)
{
    auto bytes = tempo_utils::MemoryBytes::copy(message);
    TU_LOG_INFO << "port " << m_port->getUrl() << " received message (" << bytes->getSize() << " bytes)";
    TU_LOG_FATAL << "aborting"; // TODO: FIXME!
    m_port->receive(bytes);
    return tempo_utils::GenericStatus::ok();
}

tempo_utils::Status
chord_machine::PortSocket::detach()
{
    m_writer = nullptr;
    return m_port->detach();
}

tempo_utils::Status
chord_machine::PortSocket::write(std::shared_ptr<tempo_utils::ImmutableBytes> payload)
{
    if (m_writer == nullptr)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "port is not available");
    std::string_view message((const char *) payload->getData(), payload->getSize());
    return m_writer->write(message);
}