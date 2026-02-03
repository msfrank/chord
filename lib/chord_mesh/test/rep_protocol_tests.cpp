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
#include "chord_mesh/rep_protocol.h"
#include "chord_mesh/stream_acceptor.h"

class RepProtocol : public BaseMeshFixture {
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

TEST_F(RepProtocol, WriteRawRequestAndReadProtocolReply)
{
    auto testerDirectory = tempdir->getTempdir();
    auto socketPath = testerDirectory / "test.sock";

    auto *loop = getUVLoop();
    int ret;

    chord_mesh::StreamManagerOps managerOps;
    chord_mesh::StreamManager manager(loop, streamKeypair, trustStore, managerOps);

    chord_mesh::StreamAcceptorOptions acceptorOptions;
    acceptorOptions.allowInsecure = true;
    std::shared_ptr<chord_mesh::StreamAcceptor> acceptor;
    TU_ASSIGN_OR_RAISE (acceptor, chord_mesh::StreamAcceptor::create(&manager, acceptorOptions));

    using TestRequest = chord_mesh::Message<test_generated::Request>;
    using TestReply = chord_mesh::Message<test_generated::Reply>;

    class TestRepAcceptor : public chord_mesh::RepAcceptContext<TestRequest, TestReply> {
    public:
        void error(const tempo_utils::Status &) override {}
        void cleanup() override {}

        class TestRepProtocol : public chord_mesh::AbstractRepProtocol<TestRequest,TestReply> {
        public:
            tempo_utils::Status reply(
                chord_mesh::AbstractCloseable *closeable,
                const TestRequest &request, TestReply &reply) override {
                auto root = reply.getRoot();
                root.setValue("pong!");
                closeable->shutdown();
                return {};
            }

            tempo_utils::Status validate(
                std::string_view protocolName,
                std::shared_ptr<tempo_security::X509Certificate> certificate) override
            {
                return {};
            }

            void error(const tempo_utils::Status &status) override {}
            void cleanup() override {}
        };

        std::unique_ptr<chord_mesh::AbstractRepProtocol<TestRequest, TestReply>> make() override {
            auto protocol = std::make_unique<TestRepProtocol>();
            return protocol;
        }
    };

    auto ctx = std::make_unique<TestRepAcceptor>();
    ASSERT_THAT (acceptor->listenUnix(socketPath.c_str(), 0, std::move(ctx)), tempo_test::IsOk());

    ASSERT_THAT (startUVThread(), tempo_test::IsOk()) << "failed to start UV thread";
    uv_sleep(200);

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socketPath.c_str());

    auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_LE (0, fd) << "socket() error: " << strerror(errno);
    ret = connect(fd, (sockaddr *) &addr, sizeof(addr));
    ASSERT_EQ (0, ret) << "connect() error: " << strerror(errno);

    chord_mesh::Message<test_generated::Request> request;
    request.getRoot().setValue("ping!");
    auto requestBytesResult = request.toBytes();
    ASSERT_THAT (requestBytesResult, tempo_test::IsResult());
    chord_mesh::EnvelopeBuilder builder;
    builder.setVersion(chord_mesh::EnvelopeVersion::Version1);
    builder.setPayload(requestBytesResult.getResult());
    auto envelopeBytesResult = builder.toBytes();
    ASSERT_THAT (envelopeBytesResult, tempo_test::IsResult());
    auto envelopeBytes = envelopeBytesResult.getResult();

    ASSERT_TRUE (write_entire_buffer(fd, envelopeBytes->getSpan()));

    std::vector<tu_uint8> buffer;
    ASSERT_LE (0, read_until_eof(fd, buffer));

    chord_mesh::EnvelopeParser parser;
    ASSERT_THAT (parser.pushBytes(buffer), tempo_test::IsOk());

    bool ready;
    ASSERT_THAT (parser.checkReady(ready), tempo_test::IsOk());
    ASSERT_TRUE (ready);

    chord_mesh::Envelope envelope;
    ASSERT_THAT (parser.takeReady(envelope), tempo_test::IsOk());
    auto payload = envelope.getPayload();

    TestReply reply;
    ASSERT_THAT (reply.parse(payload), tempo_test::IsOk());
    ASSERT_EQ ("pong!", reply.getRoot().getValue());

    ASSERT_THAT (stopUVThread(), tempo_test::IsOk()) << "failed to stop UV thread";
    acceptor->shutdown();

    ASSERT_EQ (0, close(fd)) << "close() error: " << strerror(errno);
}