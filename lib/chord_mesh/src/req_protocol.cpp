
#include <chord_mesh/req_protocol.h>

chord_mesh::ReqProtocolImpl::ReqProtocolImpl(
    std::shared_ptr<StreamConnector> connector,
    const ReqProtocolOptions &options)
    : m_options(options),
      m_connector(std::move(connector))
{
    TU_ASSERT (m_connector != nullptr);
}

chord_mesh::ReqProtocolImpl::ReqStreamContext::ReqStreamContext(std::weak_ptr<ReqProtocolImpl> impl)
    : m_impl(std::move(impl))
{
}

tempo_utils::Status
chord_mesh::ReqProtocolImpl::ReqStreamContext::validate(
    std::string_view protocolName,
    std::shared_ptr<tempo_security::X509Certificate> certificate)
{
    return {};
}

void
chord_mesh::ReqProtocolImpl::ReqStreamContext::receive(const Envelope &message)
{
    auto impl = m_impl.lock();
    if (impl != nullptr) {
        impl->receive(0, message.getPayload());
    }
}

void
chord_mesh::ReqProtocolImpl::ReqStreamContext::error(const tempo_utils::Status &status)
{
    auto impl = m_impl.lock();
    if (impl != nullptr) {
        impl->emitError(status);
    }
}

void
chord_mesh::ReqProtocolImpl::ReqStreamContext::cleanup()
{
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
        auto ctx = std::make_unique<ReqStreamContext>(impl);
        auto status = stream->start(std::move(ctx));
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