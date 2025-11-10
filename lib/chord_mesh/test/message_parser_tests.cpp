#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_mesh/message.h>
#include <tempo_security/ed25519_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_test/tempo_test.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/tempdir_maker.h>

#include "base_mesh_fixture.h"

class MessageParser : public BaseMeshFixture {
protected:

    void SetUp() override {
        BaseMeshFixture::SetUp();

        tempo_security::Ed25519PrivateKeyGenerator keygen;
        m_keyPair = tempo_security::GenerateUtils::generate_self_signed_key_pair(
            keygen,
            tempo_security::DigestId::None,
            "test_O",
            "test_OU",
            "edKeyPair",
            1,
            std::chrono::seconds{3600},
            std::filesystem::current_path(),
            tempo_utils::generate_name("test_ed_key_XXXXXXXX")).orElseThrow();
        ASSERT_TRUE (m_keyPair.isValid());
    }

    void TearDown() override {
        BaseMeshFixture::TearDown();
        ASSERT_TRUE(std::filesystem::remove(m_keyPair.getPemCertificateFile()));
        ASSERT_TRUE(std::filesystem::remove(m_keyPair.getPemPrivateKeyFile()));
    }

    tempo_security::CertificateKeyPair getKeyPair() const {
        return m_keyPair;
    }

private:
    tempo_security::CertificateKeyPair m_keyPair;
};

TEST_F(MessageParser, ParseUnsignedMessage)
{
    auto now = absl::Now();
    auto payload = tempo_utils::MemoryBytes::copy("hello, world!");

    chord_mesh::MessageBuilder builder;
    builder.setVersion(chord_mesh::MessageVersion::Version1);
    builder.setTimestamp(now);
    builder.setPayload(payload);

    auto toBytesResult = builder.toBytes();
    ASSERT_THAT (toBytesResult, tempo_test::IsResult());
    auto bytes = toBytesResult.getResult();

    chord_mesh::MessageParser parser;

    auto pushBytesResult = parser.pushBytes(bytes->getSpan());
    ASSERT_THAT (pushBytesResult, tempo_test::IsResult());
    auto message = pushBytesResult.getResult();

    ASSERT_EQ (absl::ToUnixSeconds(now), absl::ToUnixSeconds(message.getTimestamp()));

    auto payloadString = message.getPayload()->getStringView();
    ASSERT_EQ ("hello, world!", payloadString);

    auto digest = message.getDigest();
    ASSERT_FALSE (digest.isValid());
}

TEST_F(MessageParser, ParseSignedMessage)
{
    auto now = absl::Now();
    auto payload = tempo_utils::MemoryBytes::copy("hello, world!");
    auto keyPair = getKeyPair();

    std::shared_ptr<tempo_security::PrivateKey> privateKey;
    TU_ASSIGN_OR_RAISE (privateKey, tempo_security::PrivateKey::readFile(keyPair.getPemPrivateKeyFile()));
    std::shared_ptr<tempo_security::X509Certificate> certificate;
    TU_ASSIGN_OR_RAISE (certificate, tempo_security::X509Certificate::readFile(keyPair.getPemCertificateFile()));

    chord_mesh::MessageBuilder builder;
    builder.setVersion(chord_mesh::MessageVersion::Version1);
    builder.setTimestamp(now);
    builder.setPayload(payload);
    builder.setPrivateKey(privateKey);

    auto toBytesResult = builder.toBytes();
    ASSERT_THAT (toBytesResult, tempo_test::IsResult());
    auto bytes = toBytesResult.getResult();

    chord_mesh::MessageParser parser;
    parser.setCertificate(certificate);

    auto pushBytesResult = parser.pushBytes(bytes->getSpan());
    ASSERT_THAT (pushBytesResult, tempo_test::IsResult());
    auto message = pushBytesResult.getResult();

    ASSERT_EQ (absl::ToUnixSeconds(now), absl::ToUnixSeconds(message.getTimestamp()));

    auto payloadString = message.getPayload()->getStringView();
    ASSERT_EQ ("hello, world!", payloadString);

    auto digest = message.getDigest();
    ASSERT_TRUE (digest.isValid());
}
