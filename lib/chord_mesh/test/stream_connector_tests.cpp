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

class StreamConnector : public BaseMeshFixture {
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

TEST_F(StreamConnector, ConnectToUnixSocket)
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

    class ConnectContext : public chord_mesh::AbstractConnectContext {
    public:
        void connect(std::shared_ptr<chord_mesh::Stream> stream) override { stream->shutdown(); }
        void error(const tempo_utils::Status &status) override { TU_RAISE_IF_NOT_OK (status); }
        void cleanup() override {}
    };

    auto createConnectorResult = chord_mesh::StreamConnector::create(&manager);
    ASSERT_THAT (createConnectorResult, tempo_test::IsResult());

    struct Data {
        uv_async_t async;
        std::shared_ptr<chord_mesh::StreamConnector> connector;
        std::string socketPath;
    } data;

    data.connector = createConnectorResult.getResult();;
    data.socketPath = socketPath;
    data.async.data = &data;

    uv_async_init(loop, &data.async, [](uv_async_t *async) {
        auto *data = (Data *) async->data;
        auto ctx = std::make_unique<ConnectContext>();
        data->connector->connectUnix(data->socketPath, 0, std::move(ctx));
    });

    ASSERT_THAT (startUVThread(), tempo_test::IsOk());

    ret = uv_async_send(&data.async);
    ASSERT_EQ (0, ret) << "uv_async_send() error: " << uv_strerror(ret);

    socklen_t socklen;
    auto connfd = accept(listenfd, (sockaddr *) &addr, &socklen);
    ASSERT_LE (0, connfd) << "accept() error: " << strerror(errno);

    char buffer[512];
    ret = read(connfd, buffer, 512);
    ASSERT_EQ (0, ret) << "expected EOF but read returned " << ret;

    stopUVThread();
}

TEST_F(StreamConnector, ReadAndWaitForUnixConnectorClose)
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

    class ConnectContext : public chord_mesh::AbstractConnectContext {
    public:
        void connect(std::shared_ptr<chord_mesh::Stream> stream) override {
            chord_mesh::StreamOps streamOps;
            stream->start(streamOps);
            stream->send(chord_mesh::EnvelopeVersion::Version1, tempo_utils::MemoryBytes::copy("hello, world!"));
            stream->shutdown();
        }
        void error(const tempo_utils::Status &status) override { TU_RAISE_IF_NOT_OK (status); }
        void cleanup() override {}
    };

    chord_mesh::StreamConnectorOptions connectorOptions;
    connectorOptions.startInsecure = true;

    auto createConnectorResult = chord_mesh::StreamConnector::create(&manager, connectorOptions);
    ASSERT_THAT (createConnectorResult, tempo_test::IsResult());

    struct Data {
        uv_async_t async;
        std::shared_ptr<chord_mesh::StreamConnector> connector;
        std::string socketPath;
    } data;

    data.connector = createConnectorResult.getResult();;
    data.socketPath = socketPath;
    data.async.data = &data;

    uv_async_init(loop, &data.async, [](uv_async_t *async) {
        auto *data = (Data *) async->data;
        auto ctx = std::make_unique<ConnectContext>();
        data->connector->connectUnix(data->socketPath, 0, std::move(ctx));
    });

    ASSERT_THAT (startUVThread(), tempo_test::IsOk());

    ret = uv_async_send(&data.async);
    ASSERT_EQ (0, ret) << "uv_async_send() error: " << uv_strerror(ret);

    socklen_t socklen;
    auto connfd = accept(listenfd, (sockaddr *) &addr, &socklen);
    ASSERT_LE (0, connfd) << "accept() error: " << strerror(errno);

    tu_uint8 buffer[128];
    memset(buffer, 0, sizeof(buffer));
    ret = read(connfd, buffer, 127);
    ASSERT_LE (0, ret) << "read() error: " << strerror(errno);
    auto envelope = parse_raw_envelope(std::span(buffer, ret));
    ASSERT_EQ ("hello, world!", envelope.getPayload()->getStringView());
    ret = read(connfd, buffer, 127);
    ASSERT_EQ (0, ret) << "expected EOF but read returned " << ret;

    stopUVThread();
}

TEST_F(StreamConnector, ConnectToTcp4Socket)
{
    auto testerDirectory = tempdir->getTempdir();
    std::string ipAddress = "127.0.0.1";
    tu_uint16 tcpPort = (random() % 5000) + 25000;

    auto *loop = getUVLoop();
    int ret;

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    uv_ip4_addr(ipAddress.c_str(), tcpPort, &addr);

    auto listenfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_LE (0, listenfd) << "socket() error: " << strerror(errno);
    ret = bind(listenfd, (sockaddr *) &addr, sizeof(addr));
    ASSERT_EQ (0, ret) << "bind() error: " << strerror(errno);
    ret = listen(listenfd, 5);
    ASSERT_EQ (0, ret) << "listen() error: " << strerror(errno);
    uv_sleep(250);

    chord_mesh::StreamManagerOps managerOps;
    chord_mesh::StreamManager manager(loop, streamKeypair, trustStore, managerOps);

    class ConnectContext : public chord_mesh::AbstractConnectContext {
    public:
        void connect(std::shared_ptr<chord_mesh::Stream> stream) override { stream->shutdown(); }
        void error(const tempo_utils::Status &status) override { TU_RAISE_IF_NOT_OK (status); }
        void cleanup() override {}
    };

    auto createConnectorResult = chord_mesh::StreamConnector::create(&manager);
    ASSERT_THAT (createConnectorResult, tempo_test::IsResult());

    struct Data {
        uv_async_t async;
        std::shared_ptr<chord_mesh::StreamConnector> connector;
        std::string ipAddress;
        tu_uint16 tcpPort;
    } data;

    data.connector = createConnectorResult.getResult();;
    data.ipAddress = ipAddress;
    data.tcpPort = tcpPort;
    data.async.data = &data;

    uv_async_init(loop, &data.async, [](uv_async_t *async) {
        auto *data = (Data *) async->data;
        auto ctx = std::make_unique<ConnectContext>();
        data->connector->connectTcp4(data->ipAddress, data->tcpPort, std::move(ctx));
    });

    ASSERT_THAT (startUVThread(), tempo_test::IsOk());

    ret = uv_async_send(&data.async);
    ASSERT_EQ (0, ret) << "uv_async_send() error: " << uv_strerror(ret);

    socklen_t socklen;
    auto connfd = accept(listenfd, (sockaddr *) &addr, &socklen);
    ASSERT_LE (0, connfd) << "accept() error: " << strerror(errno);

    char buffer[512];
    ret = read(connfd, buffer, 512);
    ASSERT_EQ (0, ret) << "expected EOF but read returned " << ret;

    stopUVThread();
}

TEST_F(StreamConnector, ReadAndWaitForTcp4ConnectorClose)
{
    auto testerDirectory = tempdir->getTempdir();
    std::string ipAddress = "127.0.0.1";
    tu_uint16 tcpPort = (random() % 5000) + 25000;

    auto *loop = getUVLoop();
    int ret;

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    uv_ip4_addr(ipAddress.c_str(), tcpPort, &addr);

    auto listenfd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_LE (0, listenfd) << "socket() error: " << strerror(errno);
    ret = bind(listenfd, (sockaddr *) &addr, sizeof(addr));
    ASSERT_EQ (0, ret) << "bind() error: " << strerror(errno);
    ret = listen(listenfd, 5);
    ASSERT_EQ (0, ret) << "listen() error: " << strerror(errno);
    uv_sleep(250);

    chord_mesh::StreamManagerOps managerOps;
    chord_mesh::StreamManager manager(loop, streamKeypair, trustStore, managerOps);

    class ConnectContext : public chord_mesh::AbstractConnectContext {
    public:
        void connect(std::shared_ptr<chord_mesh::Stream> stream) override {
            chord_mesh::StreamOps streamOps;
            stream->start(streamOps);
            stream->send(chord_mesh::EnvelopeVersion::Version1, tempo_utils::MemoryBytes::copy("hello, world!"));
            stream->shutdown();
        }
        void error(const tempo_utils::Status &status) override { TU_RAISE_IF_NOT_OK (status); }
        void cleanup() override {}
    };

    chord_mesh::StreamConnectorOptions connectorOptions;
    connectorOptions.startInsecure = true;

    auto createConnectorResult = chord_mesh::StreamConnector::create(&manager, connectorOptions);
    ASSERT_THAT (createConnectorResult, tempo_test::IsResult());

    struct Data {
        uv_async_t async;
        std::shared_ptr<chord_mesh::StreamConnector> connector;
        std::string ipAddress;
        tu_uint16 tcpPort;
    } data;

    data.connector = createConnectorResult.getResult();;
    data.ipAddress = ipAddress;
    data.tcpPort = tcpPort;
    data.async.data = &data;

    uv_async_init(loop, &data.async, [](uv_async_t *async) {
        auto *data = (Data *) async->data;
        auto ctx = std::make_unique<ConnectContext>();
        data->connector->connectTcp4(data->ipAddress, data->tcpPort, std::move(ctx));
    });

    ASSERT_THAT (startUVThread(), tempo_test::IsOk());

    ret = uv_async_send(&data.async);
    ASSERT_EQ (0, ret) << "uv_async_send() error: " << uv_strerror(ret);

    socklen_t socklen;
    auto connfd = accept(listenfd, (sockaddr *) &addr, &socklen);
    ASSERT_LE (0, connfd) << "accept() error: " << strerror(errno);

    tu_uint8 buffer[128];
    memset(buffer, 0, sizeof(buffer));
    ret = read(connfd, buffer, 127);
    ASSERT_LE (0, ret) << "read() error: " << strerror(errno);
    auto envelope = parse_raw_envelope(std::span(buffer, ret));
    ASSERT_EQ ("hello, world!", envelope.getPayload()->getStringView());
    ret = read(connfd, buffer, 127);
    ASSERT_EQ (0, ret) << "expected EOF but read returned " << ret;

    stopUVThread();
}
