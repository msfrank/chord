#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_mesh/noise.h>
#include <tempo_security/ed25519_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_test/tempo_test.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/tempdir_maker.h>

#include "base_mesh_fixture.h"

class Cipher : public BaseMeshFixture {
protected:
    std::unique_ptr<tempo_utils::TempdirMaker> tempdir;
    tempo_security::CertificateKeyPair caKeypair;
    tempo_security::CertificateKeyPair streamKeypair;
    chord_mesh::StaticKeypair initiatorKeypair;
    chord_mesh::StaticKeypair responderKeypair;
    std::shared_ptr<tempo_security::X509Store> trustStore;
    std::shared_ptr<chord_mesh::Cipher> initiator;
    std::shared_ptr<chord_mesh::Cipher> responder;

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

        std::shared_ptr<chord_mesh::Handshake> initiatorHandshake;
        TU_ASSIGN_OR_RAISE (initiatorHandshake, chord_mesh::Handshake::create("Noise_KK_25519_ChaChaPoly_BLAKE2s",
            true, initiatorKeypair.privateKey, responderKeypair.publicKey));

        std::shared_ptr<chord_mesh::Handshake> responderHandshake;
        TU_ASSIGN_OR_RAISE (responderHandshake, chord_mesh::Handshake::create("Noise_KK_25519_ChaChaPoly_BLAKE2s",
            false, responderKeypair.privateKey, initiatorKeypair.publicKey));

        TU_RAISE_IF_NOT_OK (initiatorHandshake->start());
        TU_RAISE_IF_NOT_OK (responderHandshake->start());

        while (initiatorHandshake->getHandshakeState() == chord_mesh::HandshakeState::Waiting
            || responderHandshake->getHandshakeState() == chord_mesh::HandshakeState::Waiting) {

            while (initiatorHandshake->hasOutgoing()) {
                auto outgoing = initiatorHandshake->popOutgoing();
                TU_RAISE_IF_NOT_OK (responderHandshake->process(outgoing->getData(), outgoing->getSize()));
            }

            while (responderHandshake->hasOutgoing()) {
                auto outgoing = responderHandshake->popOutgoing();
                TU_RAISE_IF_NOT_OK (initiatorHandshake->process(outgoing->getData(), outgoing->getSize()));
            }
        }

        TU_ASSERT (initiatorHandshake->getHandshakeState() == chord_mesh::HandshakeState::Split);
        TU_ASSIGN_OR_RAISE (initiator, initiatorHandshake->finish());

        TU_ASSERT (responderHandshake->getHandshakeState() == chord_mesh::HandshakeState::Split);
        TU_ASSIGN_OR_RAISE (responder, responderHandshake->finish());
    }
    void TearDown() override {
        BaseMeshFixture::TearDown();
        std::filesystem::remove_all(tempdir->getTempdir());
    }
};

TEST_F(Cipher, SendAndReceive)
{
    std::string message = "hello, world!";

    ASSERT_FALSE (initiator->hasOutput());
    auto *outputBuf = chord_mesh::ArrayBuf::allocate(message);
    ASSERT_THAT (initiator->encryptOutput(outputBuf), tempo_test::IsOk());

    ASSERT_TRUE (initiator->hasOutput());
    auto output = initiator->popOutput();

    ASSERT_FALSE (responder->hasInput());
    auto inputSpan = output->getSpan();
    ASSERT_THAT (responder->decryptInput(inputSpan.data(), inputSpan.size()), tempo_test::IsOk());
    chord_mesh::free_stream_buf(output);

    ASSERT_TRUE (responder->hasInput());
    auto input = responder->popInput();
    ASSERT_EQ (message, input->getStringView());
}
