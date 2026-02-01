#include <absl/synchronization/notification.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <chord_mesh/stream_acceptor.h>
#include <chord_mesh/stream_connector.h>
#include <tempo_security/ed25519_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_test/tempo_test.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/tempdir_maker.h>

#include "base_mesh_fixture.h"

class SecureStream : public BaseMeshFixture {
protected:
    std::unique_ptr<tempo_utils::TempdirMaker> tempdir;
    tempo_security::CertificateKeyPair caKeypair;
    tempo_security::CertificateKeyPair streamKeypair;
    std::shared_ptr<tempo_security::X509Store> trustStore;

    void SetUp() override {
        BaseMeshFixture::SetUp();
        tempdir = std::make_unique<tempo_utils::TempdirMaker>(
            std::filesystem::current_path(), "tester.XXXXXXXX");
        TU_RAISE_IF_NOT_OK (tempdir->getStatus());

        tempo_security::Ed25519PrivateKeyGenerator keygen;

        caKeypair = tempo_security::GenerateUtils::generate_self_signed_ca_key_pair(
            keygen,
            tempo_security::DigestId::None,
            "test_O",
            "test_OU",
            "caKeyPair",
            1,
            std::chrono::seconds{3600},
            1,
            tempdir->getTempdir(),
            tempo_utils::generate_name("test_ca_key_XXXXXXXX")).orElseThrow();
        TU_ASSERT (caKeypair.isValid());

        streamKeypair = tempo_security::GenerateUtils::generate_key_pair(
            caKeypair,
            keygen,
            tempo_security::DigestId::None,
            "test_O",
            "test_OU",
            "streamKeyPair",
            1,
            std::chrono::seconds{3600},
            tempdir->getTempdir(),
            tempo_utils::generate_name("test_stream_key_XXXXXXXX")).orElseThrow();
        TU_ASSERT (streamKeypair.isValid());

        tempo_security::X509StoreOptions options;
        TU_ASSIGN_OR_RAISE (trustStore, tempo_security::X509Store::loadTrustedCerts(
            options, {caKeypair.getPemCertificateFile()}));
    }
    void TearDown() override {
        BaseMeshFixture::TearDown();
        std::filesystem::remove_all(tempdir->getTempdir());
    }
};

TEST_F(SecureStream, ReadAndWriteStream)
{
    auto testerDirectory = tempdir->getTempdir();
    std::string ipAddress = "127.0.0.1";
    tu_uint16 tcpPort = (random() % 5000) + 25000;

    auto *loop = getUVLoop();
    int ret;

    chord_mesh::StreamManagerOps managerOps;
    chord_mesh::StreamManager manager(loop, streamKeypair, trustStore, managerOps);
    auto createAcceptorResult = chord_mesh::StreamAcceptor::forTcp4(ipAddress.c_str(), tcpPort, &manager);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult()) << "failed to create acceptor";
    auto acceptor = createAcceptorResult.getResult();

    struct Data {
        uv_async_t async;
        std::string ipAddress;
        tu_uint16 tcpPort;
        int numRounds;
        std::shared_ptr<chord_mesh::StreamConnector> connector;
        std::shared_ptr<chord_mesh::Stream> acceptorStream;
        std::shared_ptr<chord_mesh::Stream> connectorStream;
        std::vector<chord_mesh::Envelope> acceptorReceived;
        std::vector<chord_mesh::Envelope> connectorReceived;
        absl::Notification notifyComplete;
    } data;

    chord_mesh::StreamAcceptorOps acceptorOps;
    acceptorOps.accept = [](std::shared_ptr<chord_mesh::Stream> stream, void *ptr) {
        chord_mesh::StreamOps streamOps;
        streamOps.negotiate = [](auto protocolName, auto certificate, void *) -> bool {
            return true;
        };
        streamOps.receive = [](const chord_mesh::Envelope &envelope, void *ptr) {
            auto *data = (Data *) ptr;
            data->acceptorReceived.push_back(envelope);
            auto &stream = data->acceptorStream;
            TU_RAISE_IF_NOT_OK (stream->send(
                chord_mesh::EnvelopeVersion::Version1, tempo_utils::MemoryBytes::copy("pong!")));
        };
        TU_RAISE_IF_NOT_OK (stream->start(streamOps, ptr));
        auto *data = (Data *) ptr;
        data->acceptorStream = std::move(stream);
    };

    chord_mesh::StreamAcceptorOptions acceptorOptions;
    acceptorOptions.allowInsecure = false;
    acceptorOptions.data = &data;

    ASSERT_THAT (acceptor->listen(acceptorOps, acceptorOptions), tempo_test::IsOk()) << "acceptor listen error";

    class ConnectContext : public chord_mesh::AbstractConnectContext {
    public:
        ConnectContext(Data *data) : m_data(data) {};
        void connect(std::shared_ptr<chord_mesh::Stream> stream) override {
            chord_mesh::StreamOps streamOps;
            streamOps.receive = [](const chord_mesh::Envelope &envelope, void *ptr) {
                auto *data = (Data *) ptr;
                data->connectorReceived.push_back(envelope);
                auto &stream = data->connectorStream;
                if (data->connectorReceived.size() < data->numRounds) {
                    TU_RAISE_IF_NOT_OK (stream->send(
                        chord_mesh::EnvelopeVersion::Version1, tempo_utils::MemoryBytes::copy("ping!")));
                } else {
                    stream->shutdown();
                    data->notifyComplete.Notify();
                }
            };
            TU_RAISE_IF_NOT_OK (stream->start(streamOps, m_data));
            TU_RAISE_IF_NOT_OK (stream->send(
                chord_mesh::EnvelopeVersion::Version1, tempo_utils::MemoryBytes::copy("ping!")));
            m_data->connectorStream = std::move(stream);
        }
        void error(const tempo_utils::Status &status) override { TU_RAISE_IF_NOT_OK (status); }
        void cleanup() override {}
    private:
        Data *m_data;
    };

    chord_mesh::StreamConnectorOptions connectorOptions;
    connectorOptions.startInsecure = false;
    auto createConnectorResult = chord_mesh::StreamConnector::create(&manager, connectorOptions);
    ASSERT_THAT (createConnectorResult, tempo_test::IsResult());

    data.connector = createConnectorResult.getResult();;
    data.ipAddress = ipAddress;
    data.tcpPort = tcpPort;
    data.numRounds = 3;
    data.async.data = &data;

    uv_async_init(loop, &data.async, [](uv_async_t *async) {
        auto *data = (Data *) async->data;
        auto ctx = std::make_unique<ConnectContext>(data);
        TU_RAISE_IF_STATUS (data->connector->connectTcp4(data->ipAddress, data->tcpPort, std::move(ctx)));
    });

    ASSERT_THAT (startUVThread(), tempo_test::IsOk()) << "failed to start UV thread";

    ret = uv_async_send(&data.async);
    ASSERT_EQ (0, ret) << "uv_async_send() error: " << uv_strerror(ret);

    ASSERT_TRUE (data.notifyComplete.WaitForNotificationWithTimeout(absl::Seconds(500))) << "timeout waiting for notification";

    ASSERT_THAT (stopUVThread(), tempo_test::IsOk()) << "failed to stop UV thread";
    acceptor->shutdown();

    ASSERT_EQ (data.numRounds, data.acceptorReceived.size());
    for (const auto &envelope : data.acceptorReceived) {
        ASSERT_EQ ("ping!", envelope.getPayload()->getStringView());
    }

    ASSERT_EQ (data.numRounds, data.connectorReceived.size());
    for (const auto &envelope : data.connectorReceived) {
        ASSERT_EQ ("pong!", envelope.getPayload()->getStringView());
    }
}
