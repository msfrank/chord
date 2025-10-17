#ifndef CHORD_LOCAL_MACHINE_PORT_SOCKET_H
#define CHORD_LOCAL_MACHINE_PORT_SOCKET_H

#include <chord_common/abstract_protocol_handler.h>
#include <lyric_runtime/abstract_port_writer.h>
#include <lyric_runtime/duplex_port.h>

class PortSocket : public chord_common::AbstractProtocolHandler, public lyric_runtime::AbstractPortWriter {

public:
    PortSocket(std::shared_ptr<lyric_runtime::DuplexPort> port);

    bool isAttached() override;
    tempo_utils::Status attach(chord_common::AbstractProtocolWriter *writer) override;
    tempo_utils::Status send(std::string_view message) override;
    tempo_utils::Status handle(std::string_view message) override;
    tempo_utils::Status detach() override;

    tempo_utils::Status write(std::shared_ptr<tempo_utils::ImmutableBytes> payload) override;

private:
    std::shared_ptr<lyric_runtime::DuplexPort> m_port;
    chord_common::AbstractProtocolWriter *m_writer;
};

#endif // CHORD_LOCAL_MACHINE_PORT_SOCKET_H