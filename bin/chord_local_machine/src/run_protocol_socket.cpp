//
//#include <chord_local_machine/run_protocol_socket.h>
//
//RunProtocolSocket::RunProtocolSocket(std::shared_ptr<LocalMachine> machine)
//    : m_machine(machine),
//      m_state(RunSocketState::INVALID),
//      m_startRequested(false),
//      m_initCompleted(false),
//      m_writer(nullptr)
//{
//    TU_ASSERT (m_machine != nullptr);
//}
//
//tempo_utils::Url
//RunProtocolSocket::getProtocolUri() const
//{
//    return tempo_utils::Url::fromString(kRunProtocolUri);
//}
//
//bool
//RunProtocolSocket::isAttached()
//{
//    return m_writer != nullptr;
//}
//
//tempo_utils::Status
//RunProtocolSocket::attach(chord_common::AbstractProtocolWriter *writer)
//{
//    m_writer = writer;
//    m_state = RunSocketState::READY;
//    TU_LOG_INFO << "attached RunProtocolSocket";
//    return {};
//}
//
//tempo_utils::Status
//RunProtocolSocket::send(std::string_view message)
//{
//    TU_LOG_INFO << "sending message: " << std::string(message);
//    return m_writer->write(message);
//}
//
//tempo_utils::Status
//RunProtocolSocket::handle(std::string_view message)
//{
//    TU_LOG_INFO << "received message: " << std::string(message);
//
//    if (message == "START") {
//        m_startRequested = true;
//        return start();
//    }
//    else if (message == "STOP") {
//        return stop();
//    }
//    else if (message == "SHUTDOWN") {
//        return shutdown();
//    }
//
//    return tempo_utils::GenericStatus::forCondition(
//        tempo_utils::GenericCondition::kInternalViolation, "unknown run handler operation");
//}
//
//tempo_utils::Status
//RunProtocolSocket::detach()
//{
//    m_writer = nullptr;
//    m_state = RunSocketState::INVALID;
//    TU_LOG_INFO << "detached RunProtocolSocket";
//    return {};
//}
//
//tempo_utils::Status
//RunProtocolSocket::start()
//{
//    // if we haven't received the init complete notification yet, then do nothing
//    if (!m_startRequested || !m_initCompleted)
//        return {};
//
//    return m_machine->start();
//}
//
//tempo_utils::Status
//RunProtocolSocket::stop()
//{
//    return m_machine->stop();
//}
//
//tempo_utils::Status
//RunProtocolSocket::shutdown()
//{
//    return m_machine->shutdown();
//}
//
//void
//RunProtocolSocket::notifyInitComplete()
//{
//    m_initCompleted = true;
//    start();
//}
//
//void
//RunProtocolSocket::notifyStateChanged()
//{
//    TU_LOG_INFO << "machine state changed";
//    auto state = m_machine->getRunnerState();
//    switch (state) {
//        case InterpreterRunnerState::STOPPED:
//        case InterpreterRunnerState::SHUTDOWN:
//        case InterpreterRunnerState::FAILED:
//            send("FINISHED");
//            break;
//        default:
//            break;
//    }
//}
