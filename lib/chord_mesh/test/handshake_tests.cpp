#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_mesh/handshake.h>
#include <tempo_security/ed25519_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_test/tempo_test.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/tempdir_maker.h>

#include "base_mesh_fixture.h"

class Handshake : public BaseMeshFixture {
protected:
    std::unique_ptr<tempo_utils::TempdirMaker> tempdir;
    tempo_security::CertificateKeyPair caKeypair;
    tempo_security::CertificateKeyPair streamKeypair;
    chord_mesh::StaticKeypair initiatorKeypair;
    chord_mesh::StaticKeypair responderKeypair;
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
            tempo_utils::generate_name("test_ed_key_XXXXXXXX")).orElseThrow();
        TU_ASSERT (streamKeypair.isValid());

        std::shared_ptr<tempo_security::PrivateKey> privateKey;
        TU_ASSIGN_OR_RAISE (privateKey, tempo_security::PrivateKey::readFile(streamKeypair.getPemPrivateKeyFile()));

        TU_RAISE_IF_NOT_OK (chord_mesh::generate_static_key(privateKey, initiatorKeypair));

        TU_RAISE_IF_NOT_OK (chord_mesh::generate_static_key(privateKey, responderKeypair));

        tempo_security::X509StoreOptions options;
        TU_ASSIGN_OR_RAISE (trustStore, tempo_security::X509Store::loadTrustedCerts(
            options, {caKeypair.getPemCertificateFile()}));
    }
    void TearDown() override {
        BaseMeshFixture::TearDown();
        std::filesystem::remove_all(tempdir->getTempdir());
    }
};

TEST_F(Handshake, CreateAndStartInitiatorHandshake)
{
    auto testerDirectory = tempdir->getTempdir();

    auto createHandshakeResult = chord_mesh::Handshake::create("Noise_KK_25519_ChaChaPoly_BLAKE2s",
        true, initiatorKeypair.privateKey, responderKeypair.publicKey);
    ASSERT_THAT (createHandshakeResult, tempo_test::IsResult());

    auto handshake = createHandshakeResult.getResult();
    ASSERT_EQ (chord_mesh::HandshakeState::Initial, handshake->getHandshakeState());
    ASSERT_FALSE (handshake->hasOutgoing());

    ASSERT_THAT (handshake->start(), tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::HandshakeState::Waiting, handshake->getHandshakeState());
    ASSERT_TRUE (handshake->hasOutgoing());
}

TEST_F(Handshake, CreateAndStartResponderHandshake)
{
    auto testerDirectory = tempdir->getTempdir();

    auto createHandshakeResult = chord_mesh::Handshake::create("Noise_KK_25519_ChaChaPoly_BLAKE2s",
        false, responderKeypair.privateKey, initiatorKeypair.publicKey);
    ASSERT_THAT (createHandshakeResult, tempo_test::IsResult());

    auto handshake = createHandshakeResult.getResult();
    ASSERT_EQ (chord_mesh::HandshakeState::Initial, handshake->getHandshakeState());
    ASSERT_FALSE (handshake->hasOutgoing());

    ASSERT_THAT (handshake->start(), tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::HandshakeState::Waiting, handshake->getHandshakeState());
    ASSERT_FALSE (handshake->hasOutgoing());
}

TEST_F(Handshake, PerformHandshake)
{
    auto testerDirectory = tempdir->getTempdir();

    auto initiatorHandshakeResult = chord_mesh::Handshake::create("Noise_KK_25519_ChaChaPoly_BLAKE2s",
        true, initiatorKeypair.privateKey, responderKeypair.publicKey);
    ASSERT_THAT (initiatorHandshakeResult, tempo_test::IsResult());
    auto initiator = initiatorHandshakeResult.getResult();

    auto responderHandshakeResult = chord_mesh::Handshake::create("Noise_KK_25519_ChaChaPoly_BLAKE2s",
        false, responderKeypair.privateKey, initiatorKeypair.publicKey);
    ASSERT_THAT (responderHandshakeResult, tempo_test::IsResult());
    auto responder = responderHandshakeResult.getResult();

    ASSERT_THAT (initiator->start(), tempo_test::IsOk());
    ASSERT_THAT (responder->start(), tempo_test::IsOk());

    int round = 1;
    while (initiator->getHandshakeState() == chord_mesh::HandshakeState::Waiting
        || responder->getHandshakeState() == chord_mesh::HandshakeState::Waiting) {

        while (initiator->hasOutgoing()) {
            auto outgoing = initiator->popOutgoing();
            TU_CONSOLE_OUT << round++ << " initiator --> responder";
            ASSERT_THAT (responder->process(outgoing->getData(), outgoing->getSize()), tempo_test::IsOk());
        }

        while (responder->hasOutgoing()) {
            auto outgoing = responder->popOutgoing();
            TU_CONSOLE_OUT << round++ << " initiator <-- responder";
            ASSERT_THAT (initiator->process(outgoing->getData(), outgoing->getSize()), tempo_test::IsOk());
        }
    }

    ASSERT_EQ (chord_mesh::HandshakeState::Split, initiator->getHandshakeState());
    auto initiatorCipherResult = initiator->finish();
    ASSERT_THAT (initiatorCipherResult, tempo_test::IsResult());

    ASSERT_EQ (chord_mesh::HandshakeState::Split, responder->getHandshakeState());
    auto responderCipherResult = responder->finish();
    ASSERT_THAT (responderCipherResult, tempo_test::IsResult());
}
