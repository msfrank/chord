#include <gtest/gtest.h>
#include <gmock/gmock.h>


#include <chord_mesh/message.h>
#include <tempo_security/ed25519_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_test/tempo_test.h>
#include <tempo_utils/big_endian.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/tempdir_maker.h>

#include "base_mesh_fixture.h"

class MessageBuilder : public BaseMeshFixture {
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

TEST_F(MessageBuilder, BuildUnsignedMessage)
{
    auto now = absl::Now();
    auto payload = tempo_utils::MemoryBytes::copy("hello, world!");

    chord_mesh::MessageBuilder builder;
    builder.setVersion(chord_mesh::MessageVersion::Version1);
    builder.setPayload(payload);
    builder.setTimestamp(now);

    auto toBytesResult = builder.toBytes();
    ASSERT_THAT (toBytesResult, tempo_test::IsResult());
    auto bytes = toBytesResult.getResult();
    ASSERT_LE (5, bytes->getSize());
    auto *ptr = bytes->getData();

    tu_uint8 messageVersion = tempo_utils::read_u8_and_advance(ptr);
    ASSERT_EQ (1, messageVersion);

    tu_uint8 messageFlags = tempo_utils::read_u8_and_advance(ptr);
    ASSERT_FALSE (messageFlags & chord_mesh::kMessageSignedFlag);

    tu_uint32 timestamp = tempo_utils::read_u32_and_advance(ptr);
    ASSERT_EQ (absl::ToUnixSeconds(now), timestamp);

    tu_uint32 payloadSize = tempo_utils::read_u32_and_advance(ptr);
    ASSERT_EQ (payload->getSize(), payloadSize);

    std::string_view payloadBytes((const char *) ptr, payloadSize);
    ASSERT_EQ ("hello, world!", payloadBytes);

    ptr += payloadSize;
    ASSERT_EQ (bytes->getSize(), ptr - bytes->getData());
}

TEST_F(MessageBuilder, BuildSignedMessage)
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
    builder.setPayload(payload);
    builder.setTimestamp(now);
    builder.setPrivateKey(privateKey);

    auto toBytesResult = builder.toBytes();
    ASSERT_THAT (toBytesResult, tempo_test::IsResult());
    auto bytes = toBytesResult.getResult();
    ASSERT_LE (5, bytes->getSize());
    auto *ptr = bytes->getData();

    tu_uint8 messageVersion = tempo_utils::read_u8_and_advance(ptr);
    ASSERT_EQ (1, messageVersion);

    tu_uint8 messageFlags = tempo_utils::read_u8_and_advance(ptr);
    ASSERT_TRUE (messageFlags & chord_mesh::kMessageSignedFlag);

    tu_uint32 timestamp = tempo_utils::read_u32_and_advance(ptr);
    ASSERT_EQ (absl::ToUnixSeconds(now), timestamp);

    tu_uint32 payloadSize = tempo_utils::read_u32_and_advance(ptr);
    ASSERT_EQ (payload->getSize(), payloadSize);

    std::string_view payloadBytes((const char *) ptr, payloadSize);
    ASSERT_EQ ("hello, world!", payloadBytes);

    ptr += payloadSize;
    tu_uint8 digestSize = tempo_utils::read_u8_and_advance(ptr);
    std::span<const tu_uint8> digestBytes(ptr, digestSize);

    auto verifyResult = tempo_security::DigestUtils::verify_signed_message_digest(
        std::span(bytes->getSpan().subspan(0, 10 + payloadSize)),
        tempo_security::Digest(digestBytes), certificate);
    ASSERT_THAT (verifyResult, tempo_test::IsResult());
    ASSERT_TRUE (verifyResult.getResult());

    ptr += digestSize;
    ASSERT_EQ (bytes->getSize(), ptr - bytes->getData());
}
