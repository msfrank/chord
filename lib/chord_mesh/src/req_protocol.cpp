
#include <chord_mesh/req_protocol.h>

chord_mesh::ReqProtocolImpl::ReqProtocolImpl(
    StreamManager *manager,
    ReqProtocolErrorCallback error,
    ReqProtocolCleanupCallback cleanup,
    const ReqProtocolOptions &options)
    : m_options(options),
      m_manager(manager),
      m_error(error),
      m_cleanup(cleanup)
{
    TU_ASSERT (m_manager != nullptr);
}

void
chord_mesh::on_stream_receive(const Envelope &message, void *data)
{
    auto *impl = static_cast<ReqProtocolImpl *>(data);
    impl->receive(0, message.getPayload());
}

void
chord_mesh::on_connect_complete(std::shared_ptr<Stream> stream, void *data)
{
    auto *impl = static_cast<ReqProtocolImpl *>(data);

    StreamOps ops;
    ops.receive = on_stream_receive;
    ops.error = impl->m_error;
    ops.cleanup = impl->m_cleanup;
    auto status = stream->start(ops);
    if (status.notOk()) {
        impl->emitError(status);
        return;
    }

    while (!impl->m_pending.empty()) {
        auto pending = impl->m_pending.front();
        status = stream->send(EnvelopeVersion::Version1, pending.second);
        impl->m_pending.pop();
        if (status.notOk()) {
            impl->emitError(status);
            return;
        }
    }

    impl->m_stream = std::move(stream);
}

void
chord_mesh::on_connect_error(const tempo_utils::Status &status, void *data)
{
    auto *impl = static_cast<ReqProtocolImpl *>(data);
    if (impl->m_error) {
        impl->m_error(status, data);
    } else {
        TU_LOG_ERROR << status;
    }
}

void
chord_mesh::on_cleanup(void *data)
{
    auto *impl = static_cast<ReqProtocolImpl *>(data);
    if (impl->m_cleanup) {
        impl->m_cleanup(data);
    }
}

tempo_utils::Status
chord_mesh::ReqProtocolImpl::connect(const chord_common::TransportLocation &location)
{
    StreamConnectorOps ops;
    ops.connect = on_connect_complete;
    ops.error = on_connect_error;
    ops.cleanup = on_cleanup;
    StreamConnectorOptions options;
    options.startInsecure = m_options.startInsecure;
    options.data = this;

    TU_ASSIGN_OR_RETURN (m_connector, StreamConnector::create(m_manager, ops, options));
    TU_ASSIGN_OR_RETURN (m_connectId, m_connector->connectLocation(location));
    return {};
}

tempo_utils::Result<tu_uint32>
chord_mesh::ReqProtocolImpl::send(std::shared_ptr<const tempo_utils::ImmutableBytes> payload)
{
    auto id = m_currId++;
    if (m_stream != nullptr) {
        TU_RETURN_IF_NOT_OK (m_stream->send(EnvelopeVersion::Version1, payload, absl::Now()));
    } else {
        m_pending.emplace(id, payload);
    }
    return id;
}

void
chord_mesh::ReqProtocolImpl::emitError(const tempo_utils::Status &status)
{
    if (m_error != nullptr) {
        m_error(status, m_options.data);
    } else {
        TU_LOG_ERROR << status;
    }
}

void
chord_mesh::ReqProtocolImpl::shutdown()
{
    if (m_connector != nullptr) {
        m_connector->shutdown();
        m_connector.reset();
    }
    if (m_stream != nullptr) {
        m_stream->shutdown();
        m_stream.reset();
    }
}