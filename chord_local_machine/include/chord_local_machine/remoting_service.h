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

class CommunicationStream
    : public grpc::ServerBidiReactor<
        chord_remoting::Message,
        chord_remoting::Message>,
      public chord_protocol::AbstractProtocolWriter
{
public:
    CommunicationStream();
    ~CommunicationStream();

    void OnReadDone(bool ok) override;
    void OnWriteDone(bool ok) override;
    void OnDone() override;
    tempo_utils::Status write(std::string_view message) override;

    tempo_utils::Status attachHandler(std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler);

    struct PendingWrite {
        chord_remoting::Message message;
        PendingWrite *next;
    };

private:
    absl::Mutex m_lock;
    std::shared_ptr<chord_protocol::AbstractProtocolHandler> m_handler;
    chord_remoting::Message m_incoming;
    PendingWrite *m_head;
    PendingWrite *m_tail;
};

class RemotingService : public chord_remoting::RemotingService::CallbackService {
public:
    RemotingService();
    explicit RemotingService(uv_async_t *initComplete);

    grpc::ServerBidiReactor<
        chord_remoting::Message,
        chord_remoting::Message> *
    Communicate(grpc::CallbackServerContext *context) override;

    virtual tempo_utils::Status registerProtocolHandler(
        const tempo_utils::Url &protocolUrl,
        std::shared_ptr<chord_protocol::AbstractProtocolHandler> handler,
        bool requiredAtLaunch);
    virtual bool hasProtocolHandler(const tempo_utils::Url &protocolUrl);
    virtual std::shared_ptr<chord_protocol::AbstractProtocolHandler> getProtocolHandler(
        const tempo_utils::Url &protocolUrl);

private:
    uv_async_t *m_initComplete;
    absl::Mutex m_lock;
    absl::flat_hash_map<tempo_utils::Url, std::shared_ptr<chord_protocol::AbstractProtocolHandler>> m_handlers;
    absl::flat_hash_set<tempo_utils::Url> m_attached;
    absl::flat_hash_set<tempo_utils::Url> m_requiredAtLaunch;
};

#endif // CHORD_LOCAL_MACHINE_REMOTING_SERVICE_H