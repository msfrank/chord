
#include <chord_local_machine/port_socket.h>
#include <tempo_utils/memory_bytes.h>

PortSocket::PortSocket(std::shared_ptr<lyric_runtime::DuplexPort> port)
    : m_port(port),
      m_writer(nullptr)
{
    TU_ASSERT (m_port != nullptr);
}

bool
PortSocket::isAttached()
{
    return m_writer != nullptr;
}

tempo_utils::Status
PortSocket::attach(chord_protocol::AbstractProtocolWriter *writer)
{
    m_writer = writer;
    auto status = m_port->attach(this);
    if (status.notOk())
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, status.getMessage());
    return tempo_utils::GenericStatus::ok();
}

tempo_utils::Status
PortSocket::send(std::string_view message)
{
    if (m_writer == nullptr)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "port is not available");
    TU_LOG_INFO << "port " << m_port->getUrl() << " sends message: " << std::string(message);
    return m_writer->write(message);
}

tempo_utils::Status
PortSocket::handle(std::string_view message)
{
    auto bytes = tempo_utils::MemoryBytes::copy(message);
    TU_LOG_INFO << "port " << m_port->getUrl() << " received message (" << bytes->getSize() << " bytes)";
    TU_LOG_FATAL << "aborting"; // TODO: FIXME!
    lyric_serde::LyricPatchset patchset(bytes);
    m_port->receive(patchset);
    return tempo_utils::GenericStatus::ok();
}

tempo_utils::Status
PortSocket::detach()
{
    m_writer = nullptr;
    auto status = m_port->detach();
    if (status.notOk())
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, status.getMessage());
    return tempo_utils::GenericStatus::ok();
}

tempo_utils::Status
PortSocket::write(const lyric_serde::LyricPatchset &patchset)
{
    if (m_writer == nullptr)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "port is not available");
    auto bytes = patchset.bytesView();
    std::string_view message((const char *) bytes.data(), bytes.size());
    auto status = m_writer->write(message);
    if (status.notOk())
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, status.getMessage());
    return tempo_utils::GenericStatus::ok();
}