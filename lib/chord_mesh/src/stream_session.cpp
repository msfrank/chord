
#include <chord_mesh/generated/stream_messages.capnp.h>
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/message.h>
#include <chord_mesh/stream_session.h>

chord_mesh::StreamSession::StreamSession(
    StreamHandle *handle,
    bool initiator,
    bool insecure)
    : m_handle(handle),
      m_insecure(insecure)
{
    TU_ASSERT (m_handle != nullptr);
    m_io = std::make_unique<StreamIO>(initiator, m_handle->manager, this);
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

    // if the remote end has closed then shut down the stream
    if (nread == UV_EOF) {
        handle->shutdown();
        return;
    }

    // if nread is 0 then there is no data to read
    if (nread == 0 || buf == nullptr || buf->len < nread)
        return;

    // push data into the StreamIO
    auto status = handle->session->read((const tu_uint8 *) buf->base, nread);
    std::free(buf->base);
    if (status.notOk()) {
        handle->error(status);
        return;
    }

    // process all ready messages
    handle->session->process();
}

tempo_utils::Status
chord_mesh::StreamSession::start()
{
    auto ret = uv_read_start(m_handle->stream, allocate_buffer, perform_read);
    if (ret != 0)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "uv_read_start error: {}", uv_strerror(ret));

    TU_RETURN_IF_NOT_OK (m_io->start(!m_insecure));

    if (m_io->isInitiator() && !m_insecure) {
        auto protocolName = m_handle->manager->getProtocolName();
        TU_RETURN_IF_NOT_OK (negotiate(protocolName));
    }

    return {};
}

tempo_utils::Status
chord_mesh::StreamSession::negotiate(std::string_view protocolName)
{
    if (m_handle->state != StreamState::Active)
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

tempo_utils::Status
chord_mesh::StreamSession::read(const tu_uint8 *data, ssize_t len)
{
    return m_io->read(data, len);
}

tempo_utils::Status
chord_mesh::StreamSession::process()
{
    bool ready;
    tempo_utils::Status status;

    for (;;) {
        TU_RETURN_IF_NOT_OK (m_io->checkReady(ready));
        if (!ready)
            break;

        Envelope envelope;
        TU_RETURN_IF_NOT_OK (m_io->takeReady(envelope));

        switch (envelope.getVersion()) {
            case EnvelopeVersion::Stream:
                // handle stream messages internally
                TU_RETURN_IF_NOT_OK (processStreamMessage(envelope));
                break;
            default:
                // invoke the receive callback any other message
                m_handle->receive(envelope);
                break;
        }
    }
    return {};
}

tempo_utils::Status
chord_mesh::StreamSession::processStreamMessage(const Envelope &envelope)
{
    TU_ASSERT (envelope.getVersion() == EnvelopeVersion::Stream);

    auto payload = envelope.getPayload();
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

            std::shared_ptr<tempo_security::X509Certificate> certificate;
            TU_ASSIGN_OR_RETURN (certificate, tempo_security::X509Certificate::fromString(certificateString.cStr()));

            TU_RETURN_IF_NOT_OK (m_handle->validate(protocolName, certificate));

            TU_RETURN_IF_NOT_OK (m_io->negotiateRemote(protocolString.cStr(), certificate, publicKey, digest));

            if (m_io->getIOState() == IOState::PendingLocal) {
                TU_RETURN_IF_NOT_OK (negotiate(protocolString.cStr()));
            }
            return {};
        }

        case generated::StreamMessage::Message::STREAM_HANDSHAKE: {
            auto streamHandshake = root.getMessage().getStreamHandshake();
            auto dataBytes = streamHandshake.getData().asBytes();
            std::span data(dataBytes.begin(), dataBytes.end());

            bool finished;
            TU_RETURN_IF_NOT_OK (m_io->processHandshake(data, finished));
            return {};
        }

        case generated::StreamMessage::Message::STREAM_ERROR: {
            auto streamError = root.getMessage().getStreamError();
            auto errorMessage = streamError.getMessage().asString();
            return MeshStatus::forCondition(MeshCondition::kMeshInvariant, errorMessage.cStr());
        }

        default:
            return {};
    }
}

tempo_utils::Status
chord_mesh::StreamSession::write(std::shared_ptr<const tempo_utils::ImmutableBytes> bytes)
{
    return m_io->write(std::move(bytes));
}

void
chord_mesh::write_completed(uv_write_t *req, int err)
{
    auto *streamBuf = (StreamBuf *) req->data;
    auto *handle = (StreamHandle *) req->handle->data;

    // we are done with req and streamBuf
    std::free(req);
    free_stream_buf(streamBuf);

    if (err < 0) {
        handle->error(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "perform_write error: {}", uv_strerror(err)));
    }

    // TODO: signal write completion to StreamIO
}

tempo_utils::Status
chord_mesh::StreamSession::write(StreamBuf *streamBuf)
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
