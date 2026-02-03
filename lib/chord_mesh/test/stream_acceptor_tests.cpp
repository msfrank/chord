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

class StreamAcceptor : public BaseMeshFixture {
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

TEST_F(StreamAcceptor, CreateUnixAcceptor)
{
    auto testerDirectory = tempdir->getTempdir();
    auto socketPath = testerDirectory / "test.sock";

    auto *loop = getUVLoop();

    chord_mesh::StreamManagerOps managerOps;
    chord_mesh::StreamManager manager(loop, streamKeypair, trustStore, managerOps);
    auto createAcceptorResult = chord_mesh::StreamAcceptor::create(&manager);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult());
    auto acceptor = createAcceptorResult.getResult();

    class AcceptContext : public chord_mesh::AbstractAcceptContext {
    public:
        void accept(std::shared_ptr<chord_mesh::Stream>) override {}
        void error(const tempo_utils::Status &) override {}
        void cleanup() override {}
    };

    auto ctx = std::make_unique<AcceptContext>();
    ASSERT_THAT (acceptor->listenUnix(socketPath.c_str(), 0, std::move(ctx)), tempo_test::IsOk());
    ASSERT_THAT (startUVThread(), tempo_test::IsOk());

    uv_sleep(200);
    ASSERT_EQ (chord_mesh::AcceptState::Active, acceptor->getAcceptState());

    stopUVThread();
    acceptor->shutdown();
}

TEST_F(StreamAcceptor, ConnectToUnixAcceptor)
{
    auto testerDirectory = tempdir->getTempdir();
    auto socketPath = testerDirectory / "test.sock";

    auto *loop = getUVLoop();

    chord_mesh::StreamManagerOps managerOps;
    chord_mesh::StreamManager manager(loop, streamKeypair, trustStore, managerOps);

    chord_mesh::StreamAcceptorOptions acceptorOptions;
    acceptorOptions.allowInsecure = true;
    auto createAcceptorResult = chord_mesh::StreamAcceptor::create(&manager, acceptorOptions);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult()) << "failed to create acceptor";
    auto acceptor = createAcceptorResult.getResult();

    struct Data {
        std::shared_ptr<chord_mesh::Stream> stream;
    } data;


    class AcceptContext : public chord_mesh::AbstractAcceptContext {
    public:
        AcceptContext(Data *data): m_data(data) {}
        void accept(std::shared_ptr<chord_mesh::Stream> stream) override {
            m_data->stream = std::move(stream);
        }
        void error(const tempo_utils::Status &) override {}
        void cleanup() override {}
    private:
        Data *m_data;
    };

    auto ctx = std::make_unique<AcceptContext>(&data);
    ASSERT_THAT (acceptor->listenUnix(socketPath.c_str(), 0, std::move(ctx)), tempo_test::IsOk()) << "acceptor listen error";

    ASSERT_THAT (startUVThread(), tempo_test::IsOk()) << "failed to start UV thread";
    uv_sleep(200);

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socketPath.c_str());

    auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_LE (0, fd) << "socket() error: " << strerror(errno);
    auto ret = connect(fd, (sockaddr *) &addr, sizeof(addr));
    ASSERT_EQ (0, ret) << "connect() error: " << strerror(errno);
    uv_sleep(200);

    ASSERT_THAT (stopUVThread(), tempo_test::IsOk()) << "failed to stop UV thread";
    acceptor->shutdown();

    ASSERT_TRUE (data.stream != nullptr) << "expected server stream";
    data.stream->shutdown();

    ASSERT_EQ (0, close(fd)) << "close() error: " << strerror(errno);
}

TEST_F(StreamAcceptor, ReadAndWaitForUnixAcceptorClose)
{
    auto testerDirectory = tempdir->getTempdir();
    auto socketPath = testerDirectory / "test.sock";

    auto *loop = getUVLoop();

    chord_mesh::StreamManagerOps managerOps;
    chord_mesh::StreamManager manager(loop, streamKeypair, trustStore, managerOps);

    chord_mesh::StreamAcceptorOptions acceptorOptions;
    acceptorOptions.allowInsecure = true;
    auto createAcceptorResult = chord_mesh::StreamAcceptor::create(&manager, acceptorOptions);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult()) << "failed to create acceptor";
    auto acceptor = createAcceptorResult.getResult();

    struct Data {
        std::shared_ptr<chord_mesh::Stream> stream;
    } data;

    class StreamContext : public chord_mesh::AbstractStreamContext {
    public:
        tempo_utils::Status validate(std::string_view,std::shared_ptr<tempo_security::X509Certificate>) override {
            return {};
        }
        void receive(const chord_mesh::Envelope &envelope) override {}
        void error(const tempo_utils::Status &status) override { TU_RAISE_IF_NOT_OK (status); }
        void cleanup() override {}
    };

    class AcceptContext : public chord_mesh::AbstractAcceptContext {
    public:
        AcceptContext(Data *data) : m_data(data) {}
        void accept(std::shared_ptr<chord_mesh::Stream> stream) override {
            auto ctx = std::make_unique<StreamContext>();
            stream->start(std::move(ctx));
            stream->send(chord_mesh::EnvelopeVersion::Version1, tempo_utils::MemoryBytes::copy("hello, world!"));
            stream->shutdown();
            m_data->stream = std::move(stream);
        }
        void error(const tempo_utils::Status &) override {}
        void cleanup() override {}
    private:
        Data *m_data;
    };

    auto ctx = std::make_unique<AcceptContext>(&data);
    ASSERT_THAT (acceptor->listenUnix(socketPath.c_str(), 0, std::move(ctx)), tempo_test::IsOk()) << "acceptor listen error";

    ASSERT_THAT (startUVThread(), tempo_test::IsOk()) << "failed to start UV thread";
    uv_sleep(200);

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socketPath.c_str());

    auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_LE (0, fd) << "socket() error: " << strerror(errno);
    auto ret = connect(fd, (sockaddr *) &addr, sizeof(addr));
    ASSERT_EQ (0, ret) << "connect() error: " << strerror(errno);

    tu_uint8 buffer[128];
    memset(buffer, 0, sizeof(buffer));
    ret = read(fd, buffer, 127);
    ASSERT_LE (0, ret) << "read() error: " << strerror(errno);
    auto envelope = parse_raw_envelope(std::span(buffer, ret));
    ASSERT_EQ ("hello, world!", envelope.getPayload()->getStringView());
    ret = read(fd, buffer, 127);
    ASSERT_EQ (0, ret) << "expected EOF but read returned " << ret;

    ASSERT_THAT (stopUVThread(), tempo_test::IsOk()) << "failed to stop UV thread";
    acceptor->shutdown();

    ASSERT_TRUE (data.stream != nullptr) << "expected server stream";

    ASSERT_EQ (0, close(fd)) << "close() error: " << strerror(errno);
}

TEST_F(StreamAcceptor, CreateTcp4Acceptor)
{
    auto testerDirectory = tempdir->getTempdir();
    std::string ipAddress = "127.0.0.1";
    tu_uint16 tcpPort = (random() % 5000) + 25000;

    auto *loop = getUVLoop();

    chord_mesh::StreamManagerOps managerOps;
    chord_mesh::StreamManager manager(loop, streamKeypair, trustStore, managerOps);

    chord_mesh::StreamAcceptorOptions acceptorOptions;
    acceptorOptions.allowInsecure = true;
    auto createAcceptorResult = chord_mesh::StreamAcceptor::create(&manager, acceptorOptions);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult());
    auto acceptor = createAcceptorResult.getResult();

    class AcceptContext : public chord_mesh::AbstractAcceptContext {
    public:
        void accept(std::shared_ptr<chord_mesh::Stream>) override {}
        void error(const tempo_utils::Status &) override {}
        void cleanup() override {}
    };

    auto ctx = std::make_unique<AcceptContext>();
    ASSERT_THAT (acceptor->listenTcp4(ipAddress, tcpPort, std::move(ctx)), tempo_test::IsOk());
    ASSERT_THAT (startUVThread(), tempo_test::IsOk());

    uv_sleep(200);
    ASSERT_EQ (chord_mesh::AcceptState::Active, acceptor->getAcceptState());

    stopUVThread();
    acceptor->shutdown();
}

TEST_F(StreamAcceptor, ConnectToTcp4Acceptor)
{
    auto testerDirectory = tempdir->getTempdir();
    std::string ipAddress = "127.0.0.1";
    tu_uint16 tcpPort = (random() % 5000) + 25000;

    auto *loop = getUVLoop();

    chord_mesh::StreamManagerOps managerOps;
    chord_mesh::StreamManager manager(loop, streamKeypair, trustStore, managerOps);

    chord_mesh::StreamAcceptorOptions acceptorOptions;
    acceptorOptions.allowInsecure = true;
    auto createAcceptorResult = chord_mesh::StreamAcceptor::create(&manager, acceptorOptions);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult()) << "failed to create acceptor";
    auto acceptor = createAcceptorResult.getResult();

    struct Data {
        std::shared_ptr<chord_mesh::Stream> stream;
    } data;

    class AcceptContext : public chord_mesh::AbstractAcceptContext {
    public:
        AcceptContext(Data *data): m_data(data) {}
        void accept(std::shared_ptr<chord_mesh::Stream> stream) override {
            m_data->stream = std::move(stream);
        }
        void error(const tempo_utils::Status &) override {}
        void cleanup() override {}
    private:
        Data *m_data;
    };

    auto ctx = std::make_unique<AcceptContext>(&data);
    ASSERT_THAT (acceptor->listenTcp4(ipAddress, tcpPort, std::move(ctx)), tempo_test::IsOk()) << "acceptor listen error";

    ASSERT_THAT (startUVThread(), tempo_test::IsOk()) << "failed to start UV thread";
    uv_sleep(200);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    uv_ip4_addr(ipAddress.c_str(), tcpPort, &addr);

    auto fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_LE (0, fd) << "socket() error: " << strerror(errno);
    auto ret = connect(fd, (sockaddr *) &addr, sizeof(addr));
    ASSERT_EQ (0, ret) << "connect() error: " << strerror(errno);
    uv_sleep(200);

    ASSERT_THAT (stopUVThread(), tempo_test::IsOk()) << "failed to stop UV thread";
    acceptor->shutdown();

    ASSERT_TRUE (data.stream != nullptr) << "expected server stream";
    data.stream->shutdown();

    ASSERT_EQ (0, close(fd)) << "close() error: " << strerror(errno);
}

TEST_F(StreamAcceptor, ReadAndWaitForTcp4AcceptorClose)
{
    auto testerDirectory = tempdir->getTempdir();
    std::string ipAddress = "127.0.0.1";
    tu_uint16 tcpPort = (random() % 5000) + 25000;

    auto *loop = getUVLoop();

    chord_mesh::StreamManagerOps managerOps;
    chord_mesh::StreamManager manager(loop, streamKeypair, trustStore, managerOps);

    chord_mesh::StreamAcceptorOptions acceptorOptions;
    acceptorOptions.allowInsecure = true;
    auto createAcceptorResult = chord_mesh::StreamAcceptor::create(&manager, acceptorOptions);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult()) << "failed to create acceptor";
    auto acceptor = createAcceptorResult.getResult();

    struct Data {
        std::shared_ptr<chord_mesh::Stream> stream;
    } data;

    class StreamContext : public chord_mesh::AbstractStreamContext {
    public:
        tempo_utils::Status validate(std::string_view,std::shared_ptr<tempo_security::X509Certificate>) override {
            return {};
        }
        void receive(const chord_mesh::Envelope &envelope) override {}
        void error(const tempo_utils::Status &status) override { TU_RAISE_IF_NOT_OK (status); }
        void cleanup() override {}
    };

    class AcceptContext : public chord_mesh::AbstractAcceptContext {
    public:
        AcceptContext(Data *data): m_data(data) {}
        void accept(std::shared_ptr<chord_mesh::Stream> stream) override {
            auto ctx = std::make_unique<StreamContext>();
            stream->start(std::move(ctx));
            stream->send(chord_mesh::EnvelopeVersion::Version1, tempo_utils::MemoryBytes::copy("hello, world!"));
            stream->shutdown();
            m_data->stream = std::move(stream);
        }
        void error(const tempo_utils::Status &) override {}
        void cleanup() override {}
    private:
        Data *m_data;
    };

    auto ctx = std::make_unique<AcceptContext>(&data);
    ASSERT_THAT (acceptor->listenTcp4(ipAddress, tcpPort, std::move(ctx)), tempo_test::IsOk()) << "acceptor listen error";

    ASSERT_THAT (startUVThread(), tempo_test::IsOk()) << "failed to start UV thread";
    uv_sleep(200);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    uv_ip4_addr(ipAddress.c_str(), tcpPort, &addr);

    auto fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_LE (0, fd) << "socket() error: " << strerror(errno);
    auto ret = connect(fd, (sockaddr *) &addr, sizeof(addr));
    ASSERT_EQ (0, ret) << "connect() error: " << strerror(errno);

    tu_uint8 buffer[128];
    memset(buffer, 0, sizeof(buffer));
    ret = read(fd, buffer, 127);
    ASSERT_LE (0, ret) << "read() error: " << strerror(errno);
    auto envelope = parse_raw_envelope(std::span(buffer, ret));
    ASSERT_EQ ("hello, world!", envelope.getPayload()->getStringView());
    ret = read(fd, buffer, 127);
    ASSERT_EQ (0, ret) << "expected EOF but read returned " << ret;

    ASSERT_THAT (stopUVThread(), tempo_test::IsOk()) << "failed to stop UV thread";
    acceptor->shutdown();

    ASSERT_TRUE (data.stream != nullptr) << "expected server stream";

    ASSERT_EQ (0, close(fd)) << "close() error: " << strerror(errno);
}
