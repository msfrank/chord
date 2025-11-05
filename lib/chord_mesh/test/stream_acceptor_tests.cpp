#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <chord_mesh/stream_acceptor.h>
#include <chord_mesh/stream_connector.h>
#include <tempo_config/program_config.h>
#include <tempo_test/tempo_test.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/tempdir_maker.h>

#include "base_mesh_fixture.h"

class StreamAcceptor : public BaseMeshFixture {
protected:
    std::unique_ptr<tempo_utils::TempdirMaker> tempdir;

    void SetUp() override {
        BaseMeshFixture::SetUp();
        tempdir = std::make_unique<tempo_utils::TempdirMaker>(
            std::filesystem::current_path(), "tester.XXXXXXXX");
        TU_RAISE_IF_NOT_OK (tempdir->getStatus());
    }
    void TearDown() override {
        BaseMeshFixture::TearDown();
        std::filesystem::remove_all(tempdir->getTempdir());
    }
};

TEST_F(StreamAcceptor, CreateAcceptor)
{
    auto testerDirectory = tempdir->getTempdir();
    auto socketPath = testerDirectory / "test.sock";

    auto *loop = getUVLoop();

    chord_mesh::StreamManager manager(loop);
    auto createAcceptorResult = chord_mesh::StreamAcceptor::forUnix(socketPath.c_str(), 0, &manager);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult());
    auto acceptor = createAcceptorResult.getResult();

    chord_mesh::StreamAcceptorOps ops;
    ASSERT_THAT (acceptor->listen(ops), tempo_test::IsOk());
    ASSERT_THAT (startUVThread(), tempo_test::IsOk());

    uv_sleep(500);
    ASSERT_TRUE (std::filesystem::is_socket(socketPath));

    stopUVThread();
    acceptor->shutdown();
}

TEST_F(StreamAcceptor, ConnectToAcceptor)
{
    auto testerDirectory = tempdir->getTempdir();
    auto socketPath = testerDirectory / "test.sock";

    auto *loop = getUVLoop();

    chord_mesh::StreamManager manager(loop);
    auto createAcceptorResult = chord_mesh::StreamAcceptor::forUnix(socketPath.c_str(), 0, &manager);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult()) << "failed to create acceptor";
    auto acceptor = createAcceptorResult.getResult();

    struct Data {
        std::shared_ptr<chord_mesh::Stream> stream;
    } data;

    chord_mesh::StreamAcceptorOps acceptorOps;
    acceptorOps.accept = [](std::shared_ptr<chord_mesh::Stream> stream, void *ptr) {
        auto *data = (Data *) ptr;
        data->stream = std::move(stream);
    };
    ASSERT_THAT (acceptor->listen(acceptorOps, &data), tempo_test::IsOk()) << "acceptor listen error";

    ASSERT_THAT (startUVThread(), tempo_test::IsOk()) << "failed to start UV thread";
    uv_sleep(500);

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socketPath.c_str());

    auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_LE (0, fd) << "socket() error: " << strerror(errno);
    auto ret = connect(fd, (sockaddr *) &addr, sizeof(addr));
    ASSERT_EQ (0, ret) << "connect() error: " << strerror(errno);
    uv_sleep(250);

    ASSERT_THAT (stopUVThread(), tempo_test::IsOk()) << "failed to stop UV thread";
    acceptor->shutdown();

    ASSERT_TRUE (data.stream != nullptr) << "expected server stream";
    data.stream->shutdown();

    ASSERT_EQ (0, close(fd)) << "close() error: " << strerror(errno);
}

TEST_F(StreamAcceptor, ReadAndWaitForServerClose)
{
    auto testerDirectory = tempdir->getTempdir();
    auto socketPath = testerDirectory / "test.sock";

    auto *loop = getUVLoop();

    chord_mesh::StreamManager manager(loop);
    auto createAcceptorResult = chord_mesh::StreamAcceptor::forUnix(socketPath.c_str(), 0, &manager);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult()) << "failed to create acceptor";
    auto acceptor = createAcceptorResult.getResult();

    struct Data {
        std::shared_ptr<chord_mesh::Stream> stream;
    } data;

    chord_mesh::StreamAcceptorOps acceptorOps;
    acceptorOps.accept = [](std::shared_ptr<chord_mesh::Stream> stream, void *ptr) {
        chord_mesh::StreamOps ops;
        stream->start(ops);
        stream->send(tempo_utils::MemoryBytes::copy("hello, world!"));
        stream->shutdown();
        auto *data = (Data *) ptr;
        data->stream = std::move(stream);
    };

    ASSERT_THAT (acceptor->listen(acceptorOps, &data), tempo_test::IsOk()) << "acceptor listen error";

    ASSERT_THAT (startUVThread(), tempo_test::IsOk()) << "failed to start UV thread";
    uv_sleep(500);

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socketPath.c_str());

    auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_LE (0, fd) << "socket() error: " << strerror(errno);
    auto ret = connect(fd, (sockaddr *) &addr, sizeof(addr));
    ASSERT_EQ (0, ret) << "connect() error: " << strerror(errno);

    char buffer[128];
    memset(buffer, 0, sizeof(buffer));
    ret = read(fd, buffer, 127);
    ASSERT_LE (0, ret) << "read() error: " << strerror(errno);
    ASSERT_EQ ("hello, world!", std::string_view(buffer));
    ret = read(fd, buffer, 127);
    ASSERT_EQ (0, ret) << "expected EOF";

    ASSERT_THAT (stopUVThread(), tempo_test::IsOk()) << "failed to stop UV thread";
    acceptor->shutdown();

    ASSERT_TRUE (data.stream != nullptr) << "expected server stream";

    ASSERT_EQ (0, close(fd)) << "close() error: " << strerror(errno);
}
