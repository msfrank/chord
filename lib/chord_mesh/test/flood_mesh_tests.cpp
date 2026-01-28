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
#include "chord_mesh/flood.h"

class FloodMesh : public BaseMeshFixture {
protected:
    std::unique_ptr<tempo_utils::TempdirMaker> tempdir;
    tempo_security::Ed25519PrivateKeyGenerator keygen;
    tempo_security::CertificateKeyPair caKeypair;
    std::shared_ptr<tempo_security::X509Store> trustStore;

    void SetUp() override {
        BaseMeshFixture::SetUp();
        tempdir = std::make_unique<tempo_utils::TempdirMaker>(
            std::filesystem::current_path(), "tester.XXXXXXXX");
        TU_RAISE_IF_NOT_OK (tempdir->getStatus());

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


        tempo_security::X509StoreOptions options;
        TU_ASSIGN_OR_RAISE (trustStore, tempo_security::X509Store::loadTrustedCerts(
            options, {caKeypair.getPemCertificateFile()}));
    }
    void TearDown() override {
        BaseMeshFixture::TearDown();
        std::filesystem::remove_all(tempdir->getTempdir());
    }
    tempo_security::CertificateKeyPair generateKeypair(std::string_view commonName) {
        auto keypair = tempo_security::GenerateUtils::generate_key_pair(
            caKeypair,
            keygen,
            tempo_security::DigestId::None,
            "test_O",
            "test_OU",
            commonName,
            1,
            std::chrono::seconds{3600},
            tempdir->getTempdir(),
            tempo_utils::generate_name(absl::StrCat(commonName, "_XXXXXXXX"))).orElseThrow();
        TU_ASSERT (keypair.isValid());
        return keypair;
    }
};

TEST_F(FloodMesh, CreateNetwork)
{
    auto testerDirectory = tempdir->getTempdir();

    auto *loop = getUVLoop();

    struct Data {
        std::vector<std::string> node1Peers;
        std::vector<std::string> node2Peers;
    } data;

    chord_mesh::StreamManagerOps managerOps;

    auto socketPath1 = testerDirectory / "node1.sock";
    auto endpoint1 = chord_common::TransportLocation::forUnix("node1", socketPath1);
    auto keypair1 = generateKeypair("node1");
    chord_mesh::StreamManager manager1(loop, keypair1, trustStore, managerOps);

    chord_mesh::FloodCallbacks callbacks1;
    callbacks1.join = [](auto peerId, void *ptr) {
        auto *data = (Data *) ptr;
        data->node1Peers.push_back(std::string(peerId));
        TU_CONSOLE_OUT << "node1: " << peerId << " joins";
    };

    chord_mesh::FloodOptions options1;
    options1.data = &data;

    auto createNode1Result = chord_mesh::FloodMesh::create(endpoint1, &manager1, callbacks1, options1);
    ASSERT_THAT (createNode1Result, tempo_test::IsResult());
    auto node1 = createNode1Result.getResult();

    auto socketPath2 = testerDirectory / "node2.sock";
    auto endpoint2 = chord_common::TransportLocation::forUnix("node2", socketPath2);
    auto keypair2 = generateKeypair("node2");
    chord_mesh::StreamManager manager2(loop, keypair2, trustStore, managerOps);

    chord_mesh::FloodCallbacks callbacks2;
    callbacks2.join = [](auto peerId, void *ptr) {
        auto *data = (Data *) ptr;
        data->node2Peers.push_back(std::string(peerId));
        TU_CONSOLE_OUT << "node2: " << peerId << " joins";
    };

    chord_mesh::FloodOptions options2;
    options2.data = &data;

    auto createNode2Result = chord_mesh::FloodMesh::create(endpoint2, &manager2, callbacks2, options2);
    ASSERT_THAT (createNode2Result, tempo_test::IsResult());
    auto node2 = createNode2Result.getResult();

    ASSERT_THAT (node1->addPeer(endpoint2), tempo_test::IsOk());

    ASSERT_THAT (startUVThread(), tempo_test::IsOk());
    uv_sleep(500);
    ASSERT_THAT (stopUVThread(), tempo_test::IsOk());


    ASSERT_THAT (data.node1Peers, testing::UnorderedElementsAre("node2"));
    ASSERT_THAT (data.node2Peers, testing::UnorderedElementsAre("node1"));
}
