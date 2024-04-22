#ifndef CHORD_LOCAL_MACHINE_RUN_PROTOCOL_SOCKET_H
#define CHORD_LOCAL_MACHINE_RUN_PROTOCOL_SOCKET_H

#include <chord_protocol/abstract_protocol_handler.h>

#include "local_machine.h"

constexpr char const *kRunProtocolUri = "dev.zuri.proto:run";

enum class RunSocketState {
    INVALID,
    READY,
    STARTED,
    RUNNING,
    STOPPED,
    FINISHED,
};

class RunProtocolSocket : public chord_protocol::AbstractProtocolHandler {

public:
    explicit RunProtocolSocket(std::shared_ptr<LocalMachine> machine);

    tempo_utils::Url getProtocolUri() const;

    bool isAttached() override;
    tempo_utils::Status attach(chord_protocol::AbstractProtocolWriter *writer) override;
    tempo_utils::Status send(std::string_view message) override;
    tempo_utils::Status handle(std::string_view message) override;
    tempo_utils::Status detach() override;

    void notifyInitComplete();
    void notifyStateChanged();

private:
    std::shared_ptr<LocalMachine> m_machine;
    RunSocketState m_state;
    bool m_startRequested;
    bool m_initCompleted;
    chord_protocol::AbstractProtocolWriter *m_writer;

    tempo_utils::Status start();
    tempo_utils::Status stop();
    tempo_utils::Status shutdown();
};

#endif // CHORD_LOCAL_MACHINE_RUN_PROTOCOL_SOCKET_H