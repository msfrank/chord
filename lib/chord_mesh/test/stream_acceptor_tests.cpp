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

    auto createAcceptorResult = chord_mesh::StreamAcceptor::forUnix(socketPath.c_str(), 0, loop);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult());
    auto acceptor = createAcceptorResult.getResult();

    chord_mesh::StreamAcceptorOps ops;
    ASSERT_THAT (acceptor->listen(ops), tempo_test::IsOk());
    ASSERT_THAT (startUVThread(), tempo_test::IsOk());

    sleep(1);
    ASSERT_TRUE (std::filesystem::is_socket(socketPath));

    stopUVThread();
    acceptor->shutdown();
}

struct ConnectToAcceptorData {
    std::shared_ptr<chord_mesh::Stream> server;
};

void on_accept(std::shared_ptr<chord_mesh::Stream> stream, void *ptr)
{
    auto *data = (ConnectToAcceptorData *) ptr;
    data->server = stream;
}

TEST_F(StreamAcceptor, ConnectToAcceptor)
{
    auto testerDirectory = tempdir->getTempdir();
    auto socketPath = testerDirectory / "test.sock";

    auto *loop = getUVLoop();

    auto createAcceptorResult = chord_mesh::StreamAcceptor::forUnix(socketPath.c_str(), 0, loop);
    ASSERT_THAT (createAcceptorResult, tempo_test::IsResult()) << "failed to create acceptor";
    auto acceptor = createAcceptorResult.getResult();

    ConnectToAcceptorData data;

    chord_mesh::StreamAcceptorOps acceptorOps;
    acceptorOps.accept = on_accept;
    ASSERT_THAT (acceptor->listen(acceptorOps, &data), tempo_test::IsOk()) << "acceptor listen error";

    ASSERT_THAT (startUVThread(), tempo_test::IsOk()) << "failed to start UV thread";
    sleep(1);

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, socketPath.c_str());

    auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_LE (0, fd) << "socket() error: " << strerror(errno);
    auto ret = connect(fd, (sockaddr *) &addr, sizeof(addr));
    ASSERT_EQ (0, ret) << "connect() error: " << strerror(errno);
    sleep(1);

    ASSERT_THAT (stopUVThread(), tempo_test::IsOk()) << "failed to stop UV thread";
    acceptor->shutdown();

    ASSERT_TRUE (data.server != nullptr) << "expected server stream";
    data.server->shutdown();

    ASSERT_EQ (0, close(fd)) << "close() error: " << strerror(errno);
}
