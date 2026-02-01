
#include <chord_mesh/req_protocol.h>

chord_mesh::ReqProtocolImpl::ReqProtocolImpl(std
    ::shared_ptr<StreamConnector> connector,
    const ReqProtocolOptions &options)
    : m_options(options),
      m_connector(std::move(connector))
{
    TU_ASSERT (m_connector != nullptr);
}

void
chord_mesh::on_stream_receive(const Envelope &message, void *data)
{
    auto *impl = static_cast<ReqProtocolImpl *>(data);
    impl->receive(0, message.getPayload());
}

void
chord_mesh::on_stream_error(const tempo_utils::Status &status, void *data)
{
    auto *impl = static_cast<ReqProtocolImpl *>(data);
    impl->emitError(status);
}


chord_mesh::ReqProtocolImpl::ReqConnectContext::ReqConnectContext(std::weak_ptr<ReqProtocolImpl> impl)
    : m_impl(std::move(impl))
{
}

void
chord_mesh::ReqProtocolImpl::ReqConnectContext::connect(std::shared_ptr<Stream> stream)
{
    auto impl = m_impl.lock();
    if (impl != nullptr) {
        StreamOps ops;
        ops.receive = on_stream_receive;
        ops.error = on_stream_error;
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
    } else {
        stream->shutdown();
    }
}

void
chord_mesh::ReqProtocolImpl::ReqConnectContext::error(const tempo_utils::Status &status)
{
    auto impl = m_impl.lock();
    if (impl != nullptr) {
        impl->emitError(status);
    }
}

void
chord_mesh::ReqProtocolImpl::ReqConnectContext::cleanup()
{
}

tempo_utils::Status
chord_mesh::ReqProtocolImpl::connect(const chord_common::TransportLocation &location)
{
    auto ctx = std::make_unique<ReqConnectContext>(shared_from_this());
    TU_ASSIGN_OR_RETURN (m_connect, m_connector->connectLocation(location, std::move(ctx)));
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
    TU_LOG_ERROR << status;
}

void
chord_mesh::ReqProtocolImpl::shutdown()
{
    if (m_stream != nullptr) {
        m_stream->shutdown();
        m_stream.reset();
    }
}