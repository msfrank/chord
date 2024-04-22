#include <absl/synchronization/notification.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/support/client_callback.h>

#include <chord_remoting/remoting_service_mock.grpc.pb.h>
#include <chord_sandbox/chord_isolate.h>
#include <chord_sandbox/remoting_client.h>
#include <grpcpp/security/server_credentials.h>
#include <tempo_utils/file_utilities.h>


class EchoHandler : public chord_protocol::AbstractProtocolHandler {
public:
    EchoHandler() = default;
    bool isAttached() override;
    tempo_utils::Status attach(chord_protocol::AbstractProtocolWriter *writer) override;
    tempo_utils::Status send(std::string_view message) override;
    tempo_utils::Status handle(std::string_view message) override;
    tempo_utils::Status detach() override;
    std::string waitForMessage();

private:
    chord_protocol::AbstractProtocolWriter *m_writer = nullptr;
    std::string m_incoming;
    absl::Notification m_notification;
};

bool
EchoHandler::isAttached()
{
    return m_writer != nullptr;
}

tempo_utils::Status
EchoHandler::attach(chord_protocol::AbstractProtocolWriter *writer)
{
    TU_ASSERT (m_writer == nullptr);
    m_writer = writer;
    TU_LOG_INFO << "attached writer " << writer;
    return tempo_utils::Status();
}

tempo_utils::Status
EchoHandler::send(std::string_view message)
{
    TU_ASSERT (m_writer != nullptr);
    TU_LOG_INFO << "sending message: " << std::string(message);
    return m_writer->write(message);
}

tempo_utils::Status
EchoHandler::handle(std::string_view message)
{
    TU_LOG_INFO << "received message: " << std::string(message);
    m_incoming = message;
    m_notification.Notify();
    return tempo_utils::Status();
}

tempo_utils::Status
EchoHandler::detach()
{
    if (m_writer) {
        TU_LOG_INFO << "detached writer " << m_writer;
        m_writer = nullptr;
    }
    return tempo_utils::Status();
}

std::string
EchoHandler::waitForMessage()
{
    TU_ASSERT (m_notification.WaitForNotificationWithTimeout(absl::Milliseconds(5000)));
    return m_incoming;
}

class EchoRemotingService : public chord_remoting::RemotingService::Service {
public:
    grpc::Status Communicate(
        grpc::ServerContext *context,
        grpc::ServerReaderWriter<chord_remoting::Message, chord_remoting::Message> *stream) override;
};

grpc::Status
EchoRemotingService::Communicate(
    grpc::ServerContext* context,
    grpc::ServerReaderWriter<chord_remoting::Message, chord_remoting::Message>* stream)
{
    TU_LOG_INFO << "starting Communicate";
    stream->SendInitialMetadata();
    chord_remoting::Message message;
    if (!stream->Read(&message))
        return grpc::Status(grpc::StatusCode::INTERNAL, "read failure");
    if (!stream->Write(message))
        return grpc::Status(grpc::StatusCode::INTERNAL, "write failure");
    TU_LOG_INFO << "finished Communicate";
    return grpc::Status::OK;
}

class ClientCommunicationStream : public ::testing::Test {
protected:
    void SetUp() override {
        m_sockpath = tempo_utils::generate_name("sock.XXXXXXXX");
        m_endpoint = absl::StrCat("unix:", m_sockpath.string());
        grpc::ServerBuilder builder;
        builder.AddListeningPort(m_endpoint, grpc::InsecureServerCredentials());
        builder.RegisterService(&m_service);
        m_server = builder.BuildAndStart();
    }

    void TearDown() override {
        m_server->Shutdown();
        if (exists(m_sockpath)) {
            TU_ASSERT (remove(m_sockpath));
        }
    }

    std::unique_ptr<chord_remoting::RemotingService::Stub> NewStub() {
        std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(m_endpoint,
            grpc::InsecureChannelCredentials());
        return chord_remoting::RemotingService::NewStub(channel);
    }

    std::filesystem::path m_sockpath;
    std::string m_endpoint;
    EchoRemotingService m_service;
    std::unique_ptr<grpc::Server> m_server;
};

TEST_F(ClientCommunicationStream, SendAndReceiveMessage)
{
    auto stub = NewStub();

    auto protocolUrl = tempo_utils::Url::fromString("dev.zuri.proto:null");
    auto handler = std::make_shared<EchoHandler>();
    chord_sandbox::ClientCommunicationStream stream(stub.get(), protocolUrl, handler, false);
    tempo_utils::Status status;

    status = handler->send("hello world");
    ASSERT_TRUE (status.isOk());
    auto message = handler->waitForMessage();
    ASSERT_EQ ("hello world", message);
    stub.reset();
}