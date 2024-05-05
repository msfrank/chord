#ifndef CHORD_LOCAL_MACHINE_REMOTING_SERVICE_H
#define CHORD_LOCAL_MACHINE_REMOTING_SERVICE_H

#include <queue>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <uv.h>

#include <chord_protocol/abstract_protocol_handler.h>
#include <chord_protocol/abstract_protocol_writer.h>
#include <chord_remoting/remoting_service.grpc.pb.h>
#include <tempo_utils/url.h>

#include "local_machine.h"

class CommunicateStream;
class MonitorStream;

/**
 * gRPC service implementing the RemotingService service definition.
 */
class RemotingService : public chord_remoting::RemotingService::CallbackService {
public:
    RemotingService();
    RemotingService(bool startSuspended, std::shared_ptr<LocalMachine> localMachine, uv_async_t *initComplete);

    grpc::ServerUnaryReactor *
    SuspendMachine(
        grpc::CallbackServerContext *context,
        const chord_remoting::SuspendMachineRequest *request,
        chord_remoting::SuspendMachineResult *response) override;

    grpc::ServerUnaryReactor *
    ResumeMachine(
        grpc::CallbackServerContext *context,
        const chord_remoting::ResumeMachineRequest *request,
        chord_remoting::ResumeMachineResult *response) override;

    grpc::ServerUnaryReactor *
    TerminateMachine(
        grpc::CallbackServerContext *context,
        const chord_remoting::TerminateMachineRequest *request,
        chord_remoting::TerminateMachineResult *response) override;

    grpc::ServerBidiReactor<
        chord_remoting::Message,
        chord_remoting::Message> *
    Communicate(grpc::CallbackServerContext *context) override;

    grpc::ServerWriteReactor<chord_remoting::MonitorEvent> *
    Monitor(
        grpc::CallbackServerContext *context,
        const chord_remoting::MonitorRequest *request) override;

    virtual tempo_utils::Status registerProtocolHandler(
        const tempo_utils::Url &protocolUrl,
        std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler,
        bool requiredAtLaunch);
    virtual bool hasProtocolHandler(const tempo_utils::Url &protocolUrl);
    virtual std::shared_ptr<chord_protocol::AbstractProtocolHandler> getProtocolHandler(
        const tempo_utils::Url &protocolUrl);

    virtual void notifyMachineStateChanged(chord_remoting::MachineState currState);
    virtual void notifyMachineExit(tu_int32 exitStatus);

private:
    std::shared_ptr<LocalMachine> m_localMachine;
    uv_async_t *m_initComplete;
    absl::Mutex m_lock;
    absl::flat_hash_map<
        tempo_utils::Url,
        std::shared_ptr<chord_protocol::AbstractProtocolHandler>> m_handlers ABSL_GUARDED_BY(m_lock);
    absl::flat_hash_set<tempo_utils::Url> m_requiredAtLaunch ABSL_GUARDED_BY(m_lock);
    absl::flat_hash_map<tempo_utils::Url,CommunicateStream *> m_communicateStreams ABSL_GUARDED_BY(m_lock);
    absl::flat_hash_set<MonitorStream *> m_monitorStreams ABSL_GUARDED_BY(m_lock);
    chord_remoting::MachineState m_cachedState ABSL_GUARDED_BY(m_lock);

    CommunicateStream *allocateCommunicateStream(const tempo_utils::Url &protocolUrl);
    void freeCommunicateStream(const tempo_utils::Url &protocolUrl);
    MonitorStream *allocateMonitorStream();
    void freeMonitorStream(MonitorStream *stream);

    friend class CommunicateStream;
    friend class MonitorStream;
};

/**
 * Reactor implementing the Communicate rpc.
 */
class CommunicateStream
    : public grpc::ServerBidiReactor<
        chord_remoting::Message,
        chord_remoting::Message>,
      public chord_protocol::AbstractProtocolWriter
{
public:
    CommunicateStream(const tempo_utils::Url &protocolUrl, RemotingService *remotingService);
    ~CommunicateStream() override;

    void OnReadDone(bool ok) override;
    void OnWriteDone(bool ok) override;
    void OnCancel() override;
    void OnDone() override;
    tempo_utils::Status write(std::string_view message) override;

    tempo_utils::Status attachHandler(std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler);

    struct PendingWrite {
        chord_remoting::Message message;
        PendingWrite *next;
    };

private:
    tempo_utils::Url m_protcolUrl;
    std::shared_ptr<chord_protocol::AbstractProtocolHandler> m_handler;
    chord_remoting::Message m_incoming;
    absl::Mutex m_lock;
    RemotingService *m_remotingService;
    PendingWrite *m_head ABSL_GUARDED_BY(m_lock);
    PendingWrite *m_tail ABSL_GUARDED_BY(m_lock);
};

/**
 * Reactor implementing the Monitor rpc.
 */
class MonitorStream : public grpc::ServerWriteReactor<chord_remoting::MonitorEvent> {
public:
    MonitorStream(RemotingService *remotingService, chord_remoting::MachineState currState);
    ~MonitorStream() override;

    void OnWriteDone(bool ok) override;
    void OnCancel() override;
    void OnDone() override;
    tempo_utils::Status notifyMachineStateChanged(chord_remoting::MachineState currState);
    tempo_utils::Status notifyMachineExit(tu_int32 exitStatus);

    struct PendingWrite {
        chord_remoting::MonitorEvent event;
        PendingWrite *next;
    };

private:
    absl::Mutex m_lock;
    RemotingService *m_remotingService;
    PendingWrite *m_head ABSL_GUARDED_BY(m_lock);
    PendingWrite *m_tail ABSL_GUARDED_BY(m_lock);

    tempo_utils::Status enqueueWrite(chord_remoting::MonitorEvent &&event);
};

#endif // CHORD_LOCAL_MACHINE_REMOTING_SERVICE_H