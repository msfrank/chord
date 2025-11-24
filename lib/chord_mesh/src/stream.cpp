
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/common.h>
#include <kj/io.h>

#include <chord_mesh/generated/stream_messages.capnp.h>
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream.h>
#include <chord_mesh/stream_io.h>
#include <tempo_utils/big_endian.h>

#include "chord_mesh/noise.h"

chord_mesh::Stream::Stream(StreamHandle *handle, bool initiator, bool secure)
    : m_handle(handle),
      m_initiator(initiator),
      m_secure(secure),
      m_id(tempo_utils::UUID::randomUUID()),
      m_state(StreamState::Initial),
      m_data(nullptr)
{
    TU_ASSERT (m_handle != nullptr);
    m_handle->data = this;
    m_io = std::make_unique<StreamIO>(m_initiator, m_handle->manager, this);
}

chord_mesh::Stream::~Stream()
{
    shutdown();
}

bool
chord_mesh::Stream::isInitiator() const
{
    return m_initiator;
}

bool
chord_mesh::Stream::isSecure() const
{
    return m_secure;
}

tempo_utils::UUID
chord_mesh::Stream::getId() const
{
    return m_id;
}

chord_mesh::StreamState
chord_mesh::Stream::getStreamState() const
{
    return m_state;
}

void
chord_mesh::allocate_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    auto size = suggested_size < 4096? suggested_size : 4096;
    buf->base = (char *) std::malloc(size);
    buf->len = size;
}

void
chord_mesh::perform_read(uv_stream_t *s, ssize_t nread, const uv_buf_t *buf)
{
    auto *handle = (StreamHandle *) s->data;
    auto *stream = (Stream *) handle->data;
    auto &io = stream->m_io;

    // if the remote end has closed then shut down the stream
    if (nread == UV_EOF) {
        stream->shutdown();
        return;
    }

    // if nread is 0 then there is no data to read
    if (nread == 0 || buf == nullptr || buf->len < nread)
        return;

    // push data into the StreamIO
    auto status = io->read((const tu_uint8 *) buf->base, nread);
    std::free(buf->base);
    if (status.notOk()) {
        stream->emitError(status);
        return;
    }

    // handle any ready messages
    stream->processReadyMessages();
}

tempo_utils::Status
chord_mesh::Stream::start(const StreamOps &ops, void *data)
{
    if (m_state != StreamState::Initial)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream state");

    auto ret = uv_read_start(m_handle->stream, allocate_buffer, perform_read);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_read_start error: {}", uv_strerror(ret));

    TU_RETURN_IF_NOT_OK (m_io->start(m_secure));

    m_state = StreamState::Active;
    m_ops = ops;
    m_data = data;

    if (m_initiator && m_secure) {
        auto protocolName = m_handle->manager->getProtocolName();
        TU_RETURN_IF_NOT_OK (negotiate(protocolName));
    }

    return {};
}

tempo_utils::Status
chord_mesh::Stream::negotiate(std::string_view protocolName)
{
    if (m_state != StreamState::Active)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "invalid stream state");

    auto *manager = m_handle->manager;
    auto keypair = manager->getKeypair();

    std::shared_ptr<tempo_security::X509Certificate> certificate;
    TU_ASSIGN_OR_RETURN (certificate, tempo_security::X509Certificate::readFile(keypair.getPemCertificateFile()));
    std::shared_ptr<tempo_security::PrivateKey> privateKey;
    TU_ASSIGN_OR_RETURN (privateKey, tempo_security::PrivateKey::readFile(keypair.getPemPrivateKeyFile()));

    // generate the noise keypair and sign it using the private key
    StaticKeypair localKeypair;
    TU_RETURN_IF_NOT_OK (generate_static_key(privateKey, localKeypair));

    // perform the local handshake
    TU_RETURN_IF_NOT_OK (m_io->negotiateLocal(protocolName, certificate, localKeypair));

    return {};
}

void
chord_mesh::write_completed(uv_write_t *req, int err)
{
    auto *streamBuf = (StreamBuf *) req->data;
    auto *handle = (StreamHandle *) req->handle->data;
    auto *stream = (Stream *) handle->data;

    // we are done with req and streamBuf
    std::free(req);
    free_stream_buf(streamBuf);

    if (err < 0) {
        stream->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "perform_write error: {}", uv_strerror(err)));
        return;
    }

    // TODO: signal write completion to StreamIO
}

tempo_utils::Status
chord_mesh::Stream::write(StreamBuf *streamBuf)
{
    // construct write req which will be freed in write_completed callback
    auto *req = (uv_write_t *) std::malloc(sizeof(uv_write_t));
    memset(req, 0, sizeof(uv_write_t));
    req->data = streamBuf;

    auto ret = uv_write(req, m_handle->stream, &streamBuf->buf, 1, write_completed);
    if (ret != 0) {
        std::free(req); // we free req but leave streamBuf untouched
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "write error: {}", uv_strerror(ret));
    }
    return {};
}

tempo_utils::Status
chord_mesh::Stream::send(
    MessageVersion version,
    std::shared_ptr<const tempo_utils::ImmutableBytes> payload,
    absl::Time timestamp)
{
    if (m_state == StreamState::Closed)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "stream is closed");
    MessageBuilder builder;
    builder.setVersion(version);
    builder.setPayload(payload);
    builder.setTimestamp(timestamp);
    std::shared_ptr<const tempo_utils::ImmutableBytes> message;
    TU_ASSIGN_OR_RETURN (message, builder.toBytes());
    return m_io->write(message);
}

void
chord_mesh::Stream::processReadyMessages()
{
    bool ready;
    tempo_utils::Status status;

    for (;;) {
        status = m_io->checkReady(ready);
        if (status.notOk()) {
            emitError(status);
            return;
        }

        if (!ready)
            break;

        Message message;
        status = m_io->takeReady(message);
        if (status.notOk()) {
            emitError(status);
            return;
        }

        switch (message.getVersion()) {
            case MessageVersion::Stream:
                // handle stream messages internally
                processStreamMessage(message);
                break;
            default:
                // invoke the receive callback any other message
                if (m_ops.receive != nullptr) {
                    m_ops.receive(message, m_data);
                }
                break;
        }
    }
}

void
chord_mesh::Stream::processStreamMessage(const Message &message)
{
    TU_ASSERT (message.getVersion() == MessageVersion::Stream);

    auto payload = message.getPayload();
    auto arrayPtr = kj::arrayPtr(payload->getData(), payload->getSize());
    kj::ArrayInputStream inputStream(arrayPtr);
    capnp::MallocMessageBuilder builder;
    capnp::readMessageCopy(inputStream, builder);

    auto root = builder.getRoot<generated::StreamMessage>();
    switch (root.getMessage().which()) {

        case generated::StreamMessage::Message::STREAM_NEGOTIATE: {
            auto streamNegotiate = root.getMessage().getStreamNegotiate();
            auto protocolString = streamNegotiate.getProtocol().asString();;
            auto publicKeyBytes = streamNegotiate.getPublicKey().asBytes();
            auto certificateString = streamNegotiate.getCertificate().asString();;
            auto digestBytes = streamNegotiate.getDigest().asBytes();

            std::string_view protocolName(protocolString.cStr());
            std::span publicKey(publicKeyBytes.begin(), publicKeyBytes.end());
            tempo_security::Digest digest(std::span(digestBytes.begin(), digestBytes.end()));

            auto readCertificateResult = tempo_security::X509Certificate::fromString(certificateString.cStr());
            if (readCertificateResult.isStatus()) {
                emitError(readCertificateResult.getStatus());
                return;
            }
            auto certificate = readCertificateResult.getResult();


            if (m_ops.negotiate != nullptr) {
                if (!m_ops.negotiate(protocolName, certificate, m_data)) {
                    emitError(MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                        "negotiation was rejected"));
                    return;
                }
            }

            tempo_utils::Status status;

            status = m_io->negotiateRemote(protocolString.cStr(), certificate, publicKey, digest);
            if (status.notOk()) {
                emitError(status);
            }

            if (m_io->getIOState() == IOState::PendingLocal) {
                status = negotiate(protocolString.cStr());
                if (status.notOk()) {
                    emitError(status);
                }
            }

            break;
        }

        case generated::StreamMessage::Message::STREAM_HANDSHAKE: {
            auto streamHandshake = root.getMessage().getStreamHandshake();
            auto dataBytes = streamHandshake.getData().asBytes();
            std::span data(dataBytes.begin(), dataBytes.end());

            bool finished;
            auto status = m_io->processHandshake(data, finished);
            if (status.notOk()) {
                emitError(status);
            }
            break;
        }

        case generated::StreamMessage::Message::STREAM_ERROR: {
            auto streamError = root.getMessage().getStreamError();
            auto errorMessage = streamError.getMessage().asString();
            emitError(MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                errorMessage.cStr()));
            break;
        }

        default:
            break;
    }
}

void
chord_mesh::Stream::shutdown()
{
    switch (m_state) {
        case StreamState::Initial:
            m_handle->close();
            break;
        case StreamState::Active:
            m_handle->shutdown();
            break;
        default:
            return;
    }

    m_state = StreamState::Closed;

    if (m_ops.cleanup != nullptr) {
        m_ops.cleanup(m_data);
    }
}

void
chord_mesh::Stream::emitError(const tempo_utils::Status &status)
{
    if (m_ops.error != nullptr) {
        m_ops.error(status, m_data);
    }
}