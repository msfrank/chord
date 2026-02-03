#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <chord_mesh/stream_connector.h>
#include <tempo_security/ed25519_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_test/tempo_test.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/tempdir_maker.h>

#include "base_mesh_fixture.h"
#include "chord_mesh/message.h"
#include "chord_mesh/req_protocol.h"
#include "test_messages.capnp.h"

class ReqProtocol : public BaseMeshFixture {
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

TEST_F(ReqProtocol, ReadAndWaitForUnixConnectorClose)
{
    auto testerDirectory = tempdir->getTempdir();
    auto socketPath = testerDirectory / "test.sock";

    auto *loop = getUVLoop();
    int ret;

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socketPath.c_str());

    auto listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_LE (0, listenfd) << "socket() error: " << strerror(errno);
    ret = bind(listenfd, (sockaddr *) &addr, sizeof(addr));
    ASSERT_EQ (0, ret) << "bind() error: " << strerror(errno);
    ret = listen(listenfd, 5);
    ASSERT_EQ (0, ret) << "listen() error: " << strerror(errno);
    uv_sleep(250);

    chord_mesh::StreamManagerOps managerOps;
    chord_mesh::StreamManager manager(loop, streamKeypair, trustStore, managerOps);

    chord_mesh::StreamConnectorOptions connectorOptions;
    connectorOptions.startInsecure = true;
    std::shared_ptr<chord_mesh::StreamConnector> connector;
    TU_ASSIGN_OR_RAISE (connector, chord_mesh::StreamConnector::create(&manager, connectorOptions));

    using TestRequest = chord_mesh::Message<test_generated::Request>;
    using TestReply = chord_mesh::Message<test_generated::Reply>;
    using TestReqProtocol = chord_mesh::ReqProtocol<TestRequest,TestReply>;

    class TestReqContext : public TestReqProtocol::AbstractContext {
    public:
        void ready(TestReqProtocol *protocol) override {
            TestRequest msg;
            auto root = msg.getRoot();
            root.setValue("hello, world!");
            protocol->send(std::move(msg));
            protocol->shutdown();
        }
        void receive(TestReqProtocol *protocol, const chord_mesh::Message<test_generated::Reply> &message) override {}
        void error(const tempo_utils::Status &status) override {
            TU_CONSOLE_ERR << "req error: " << status;
        };
        void cleanup() override {};
    };

    auto ctx = std::make_unique<TestReqContext>();

    auto createReqResult = TestReqProtocol::create(connector, std::move(ctx));
    ASSERT_THAT (createReqResult, tempo_test::IsResult());

    struct Data {
        std::shared_ptr<TestReqProtocol> req;
        chord_common::TransportLocation endpoint;
        uv_async_t async;
    } data;

    data.req = createReqResult.getResult();;
    data.endpoint = chord_common::TransportLocation::forUnix("", socketPath);
    data.async.data = &data;

    uv_async_init(loop, &data.async, [](uv_async_t *async) {
        auto *data = (Data *) async->data;
        data->req->connect(data->endpoint);
    });

    ASSERT_THAT (startUVThread(), tempo_test::IsOk());

    ret = uv_async_send(&data.async);
    ASSERT_EQ (0, ret) << "uv_async_send() error: " << uv_strerror(ret);

    socklen_t socklen;
    auto connfd = accept(listenfd, (sockaddr *) &addr, &socklen);
    ASSERT_LE (0, connfd) << "accept() error: " << strerror(errno);

    std::vector<tu_uint8> buffer;
    ASSERT_LE (0, read_until_eof(connfd, buffer));

    chord_mesh::EnvelopeParser parser;
    ASSERT_THAT (parser.pushBytes(buffer), tempo_test::IsOk());

    bool ready;
    ASSERT_THAT (parser.checkReady(ready), tempo_test::IsOk());
    ASSERT_TRUE (ready);

    chord_mesh::Envelope envelope;
    ASSERT_THAT (parser.takeReady(envelope), tempo_test::IsOk());
    auto payload = envelope.getPayload();

    TestRequest request;
    ASSERT_THAT (request.parse(payload), tempo_test::IsOk());

    stopUVThread();
}
