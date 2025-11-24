#include <capnp/message.h>
#include <capnp/serialize.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_mesh/message.h>
#include <kj/common.h>
#include <kj/io.h>
#include <tempo_security/ed25519_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_test/tempo_test.h>
#include <tempo_utils/big_endian.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/tempdir_maker.h>

#include "base_mesh_fixture.h"
#include "test_mocks.h"
#include "chord_mesh/stream_io.h"
#include "chord_mesh/generated/stream_messages.capnp.h"

class StreamIO : public BaseMeshFixture {
protected:
    std::unique_ptr<tempo_utils::TempdirMaker> tempdir;
    tempo_security::CertificateKeyPair caKeypair;
    tempo_security::CertificateKeyPair streamKeypair;
    std::shared_ptr<tempo_security::X509Certificate> certificate;
    std::shared_ptr<tempo_security::PrivateKey> privateKey;
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
            tempo_utils::generate_name("test_stream_key_XXXXXXXX")).orElseThrow();
        TU_ASSERT (streamKeypair.isValid());

        TU_ASSIGN_OR_RAISE (certificate, tempo_security::X509Certificate::readFile(streamKeypair.getPemCertificateFile()));

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

TEST_F(StreamIO, CreateAndStartInitiator)
{
    chord_mesh::StreamManager manager(getUVLoop(), streamKeypair, trustStore, {});
    MockStreamBufWriter streamBufWriter;

    chord_mesh::StreamIO streamIO(true, &manager, &streamBufWriter);
    ASSERT_EQ (chord_mesh::IOState::Initial, streamIO.getIOState());

    ASSERT_THAT (streamIO.start(false), tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::IOState::Insecure, streamIO.getIOState());

    bool ready;
    ASSERT_THAT (streamIO.checkReady(ready), tempo_test::IsOk());
    ASSERT_FALSE (ready);

    EXPECT_CALL (streamBufWriter, write(::testing::_))
        .Times(0);
}

TEST_F(StreamIO, CreateAndStartResponder)
{
    chord_mesh::StreamManager manager(getUVLoop(), streamKeypair, trustStore, {});
    MockStreamBufWriter streamBufWriter;

    chord_mesh::StreamIO streamIO(false, &manager, &streamBufWriter);
    ASSERT_EQ (chord_mesh::IOState::Initial, streamIO.getIOState());

    ASSERT_THAT (streamIO.start(false), tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::IOState::Insecure, streamIO.getIOState());

    bool ready;
    ASSERT_THAT (streamIO.checkReady(ready), tempo_test::IsOk());
    ASSERT_FALSE (ready);

    EXPECT_CALL (streamBufWriter, write(::testing::_))
        .Times(0);
}

TEST_F(StreamIO, SendAndReceiveInsecure)
{
    chord_mesh::StreamManager manager(getUVLoop(), streamKeypair, trustStore, {});
    MockStreamBufWriter streamBufWriter;

    chord_mesh::StreamIO streamIO(false, &manager, &streamBufWriter);
    ASSERT_EQ (chord_mesh::IOState::Initial, streamIO.getIOState());

    ASSERT_THAT (streamIO.start(false), tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::IOState::Insecure, streamIO.getIOState());

    chord_mesh::StreamBuf *writeBuf = nullptr;
    EXPECT_CALL (streamBufWriter, write(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke([&](auto *writeBuf_) -> tempo_utils::Status {
            writeBuf = writeBuf_;
            return {};
        }));

    std::string hello = "hello!";
    auto *streamBuf = chord_mesh::ArrayBuf::allocate(hello);
    ASSERT_THAT (streamIO.write(streamBuf), tempo_test::IsOk());
    ASSERT_EQ (hello, writeBuf->getStringView());
    chord_mesh::free_stream_buf(writeBuf);

    std::string goodbye = "goodbye!";
    auto now = absl::Now();

    chord_mesh::MessageBuilder builder;
    builder.setVersion(chord_mesh::MessageVersion::Version1);
    builder.setTimestamp(now);
    builder.setPayload(tempo_utils::MemoryBytes::copy(goodbye));
    auto toBytesResult = builder.toBytes();
    ASSERT_THAT (toBytesResult, tempo_test::IsResult());
    auto bytes = toBytesResult.getResult();

    ASSERT_THAT (streamIO.read(bytes->getData(), bytes->getSize()), tempo_test::IsOk());

    bool ready;
    ASSERT_THAT (streamIO.checkReady(ready), tempo_test::IsOk());
    ASSERT_TRUE (ready);

    chord_mesh::Message message;
    ASSERT_THAT (streamIO.takeReady(message), tempo_test::IsOk());
    ASSERT_TRUE (message.isValid());
    ASSERT_FALSE (message.isSigned());
    ASSERT_EQ (chord_mesh::MessageVersion::Version1, message.getVersion());
    ASSERT_EQ (absl::ToUnixSeconds(now), absl::ToUnixSeconds(message.getTimestamp()));
    ASSERT_EQ (goodbye, message.getPayload()->getStringView());
}

class TestStreamBufWriter : public chord_mesh::AbstractStreamBufWriter {
public:
    std::queue<chord_mesh::StreamBuf *> bufs;
    tempo_utils::Status write(chord_mesh::StreamBuf *buf) override {
        bufs.push(buf);
        return {};
    }
};

chord_mesh::generated::StreamMessage::Builder
parse_stream_message(const chord_mesh::Message &message)
{
    auto payload = message.getPayload();
    auto arrayPtr = kj::arrayPtr(payload->getData(), payload->getSize());
    kj::ArrayInputStream inputStream(arrayPtr);
    capnp::MallocMessageBuilder builder;
    capnp::readMessageCopy(inputStream, builder);
    return builder.getRoot<chord_mesh::generated::StreamMessage>();
}

std::shared_ptr<const tempo_utils::ImmutableBytes>
parse_handshake_message(std::span<const tu_uint8> data)
{
    chord_mesh::MessageParser parser;
    TU_RAISE_IF_NOT_OK (parser.pushBytes(data));
    bool ready;
    TU_RAISE_IF_NOT_OK (parser.checkReady(ready));
    if (!ready)
        return {};
    chord_mesh::Message message;
    TU_RAISE_IF_NOT_OK (parser.takeReady(message));
    auto payload = message.getPayload();
    auto arrayPtr = kj::arrayPtr(payload->getData(), payload->getSize());
    kj::ArrayInputStream inputStream(arrayPtr);
    capnp::MallocMessageBuilder builder;
    capnp::readMessageCopy(inputStream, builder);
    auto streamHandshake = builder
        .getRoot<chord_mesh::generated::StreamMessage>()
        .getMessage()
        .getStreamHandshake();
    auto handshakeData = streamHandshake.getData();
    return tempo_utils::MemoryBytes::copy(std::span(handshakeData.begin(), handshakeData.size()));
}

TEST_F(StreamIO, PerformInitiatorHandshake)
{
    chord_mesh::StreamManager manager(getUVLoop(), streamKeypair, trustStore, {});
    TestStreamBufWriter streamBufWriter;

    chord_mesh::StreamIO streamIO(true, &manager, &streamBufWriter);
    ASSERT_EQ (chord_mesh::IOState::Initial, streamIO.getIOState());

    ASSERT_THAT (streamIO.start(false), tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::IOState::Insecure, streamIO.getIOState());

    ASSERT_THAT (streamIO.negotiateLocal(chord_mesh::kDefaultNoiseProtocol, certificate, initiatorKeypair),
        tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::IOState::PendingRemote, streamIO.getIOState());

    ASSERT_EQ (1, streamBufWriter.bufs.size());
    auto *buf1 = streamBufWriter.bufs.front();
    streamBufWriter.bufs.pop();

    chord_mesh::MessageParser parser;
    ASSERT_THAT (parser.pushBytes(buf1->getSpan()), tempo_test::IsOk());
    chord_mesh::free_stream_buf(buf1);
    bool ready;
    ASSERT_THAT (parser.checkReady(ready), tempo_test::IsOk());
    chord_mesh::Message message1;
    ASSERT_THAT (parser.takeReady(message1), tempo_test::IsOk());

    auto streamMessage1 = parse_stream_message(message1);
    ASSERT_EQ (chord_mesh::generated::StreamMessage::Message::Which::STREAM_NEGOTIATE, streamMessage1.getMessage().which());
    auto streamNegotiate = streamMessage1.getMessage().getStreamNegotiate();
    ASSERT_EQ (std::string_view(chord_mesh::kDefaultNoiseProtocol), streamNegotiate.getProtocol().cStr());

    ASSERT_THAT (streamIO.negotiateRemote(chord_mesh::kDefaultNoiseProtocol, certificate,
        responderKeypair.publicKey, responderKeypair.digest), tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::IOState::Handshaking, streamIO.getIOState());

    std::shared_ptr<chord_mesh::Handshake> responderHandshake;
    TU_ASSIGN_OR_RAISE (responderHandshake, chord_mesh::Handshake::forResponder(
        chord_mesh::kDefaultNoiseProtocol, responderKeypair.privateKey, initiatorKeypair.publicKey));
    ASSERT_THAT (responderHandshake->start(), tempo_test::IsOk());

    bool handshakeFinished = false;

    while (streamIO.getIOState() == chord_mesh::IOState::Handshaking
        || responderHandshake->getHandshakeState() == chord_mesh::HandshakeState::Waiting) {
        while (!streamBufWriter.bufs.empty()) {
            auto *buf = streamBufWriter.bufs.front();
            streamBufWriter.bufs.pop();
            auto handshakeBytes = parse_handshake_message(buf->getSpan());
            chord_mesh::free_stream_buf(buf);
            ASSERT_THAT (responderHandshake->process(handshakeBytes->getData(), handshakeBytes->getSize()),
                tempo_test::IsOk());
        }
        while (!handshakeFinished && responderHandshake->hasOutgoing()) {
            auto outgoing = responderHandshake->popOutgoing();
            ASSERT_THAT (streamIO.processHandshake(outgoing->getSpan(), handshakeFinished),
                tempo_test::IsOk());
        }
    }

    ASSERT_EQ (chord_mesh::IOState::Secure, streamIO.getIOState());

    ASSERT_TRUE (handshakeFinished);
    auto finishHandshakeResult = responderHandshake->finish();
    ASSERT_THAT (finishHandshakeResult, tempo_test::IsResult());
    auto cipher = finishHandshakeResult.getResult();
}

TEST_F(StreamIO, PerformResponderHandshake)
{
    chord_mesh::StreamManager manager(getUVLoop(), streamKeypair, trustStore, {});
    TestStreamBufWriter streamBufWriter;

    chord_mesh::StreamIO streamIO(false, &manager, &streamBufWriter);
    ASSERT_EQ (chord_mesh::IOState::Initial, streamIO.getIOState());

    ASSERT_THAT (streamIO.start(false), tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::IOState::Insecure, streamIO.getIOState());

    std::shared_ptr<chord_mesh::Handshake> initiatorHandshake;
    TU_ASSIGN_OR_RAISE (initiatorHandshake, chord_mesh::Handshake::forInitiator(
        chord_mesh::kDefaultNoiseProtocol, initiatorKeypair.privateKey, responderKeypair.publicKey));
    ASSERT_THAT (initiatorHandshake->start(), tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::HandshakeState::Waiting, initiatorHandshake->getHandshakeState());

    ASSERT_THAT (streamIO.negotiateRemote(chord_mesh::kDefaultNoiseProtocol, certificate,
        initiatorKeypair.publicKey, initiatorKeypair.digest), tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::IOState::PendingLocal, streamIO.getIOState());

    ASSERT_THAT (streamIO.negotiateLocal(chord_mesh::kDefaultNoiseProtocol, certificate, responderKeypair),
        tempo_test::IsOk());
    ASSERT_EQ (chord_mesh::IOState::Handshaking, streamIO.getIOState());

    ASSERT_EQ (1, streamBufWriter.bufs.size());
    auto *buf1 = streamBufWriter.bufs.front();
    streamBufWriter.bufs.pop();

    chord_mesh::MessageParser parser;
    ASSERT_THAT (parser.pushBytes(buf1->getSpan()), tempo_test::IsOk());
    chord_mesh::free_stream_buf(buf1);
    bool ready;
    ASSERT_THAT (parser.checkReady(ready), tempo_test::IsOk());
    chord_mesh::Message message1;
    ASSERT_THAT (parser.takeReady(message1), tempo_test::IsOk());

    auto streamMessage1 = parse_stream_message(message1);
    ASSERT_EQ (chord_mesh::generated::StreamMessage::Message::Which::STREAM_NEGOTIATE, streamMessage1.getMessage().which());
    auto streamNegotiate = streamMessage1.getMessage().getStreamNegotiate();
    ASSERT_EQ (std::string_view(chord_mesh::kDefaultNoiseProtocol), streamNegotiate.getProtocol().cStr());

    bool handshakeFinished = false;

    while (streamIO.getIOState() == chord_mesh::IOState::Handshaking
        || initiatorHandshake->getHandshakeState() == chord_mesh::HandshakeState::Waiting) {
        while (!handshakeFinished && initiatorHandshake->hasOutgoing()) {
            auto outgoing = initiatorHandshake->popOutgoing();
            ASSERT_THAT (streamIO.processHandshake(outgoing->getSpan(), handshakeFinished),
                tempo_test::IsOk());
        }
        while (!streamBufWriter.bufs.empty()) {
            auto *buf = streamBufWriter.bufs.front();
            streamBufWriter.bufs.pop();
            auto handshakeBytes = parse_handshake_message(buf->getSpan());
            chord_mesh::free_stream_buf(buf);
            ASSERT_THAT (initiatorHandshake->process(handshakeBytes->getData(), handshakeBytes->getSize()),
                tempo_test::IsOk());
        }
    }

    ASSERT_EQ (chord_mesh::IOState::Secure, streamIO.getIOState());

    ASSERT_TRUE (handshakeFinished);
    auto finishHandshakeResult = initiatorHandshake->finish();
    ASSERT_THAT (finishHandshakeResult, tempo_test::IsResult());
    auto cipher = finishHandshakeResult.getResult();
}
