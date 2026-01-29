
#include <chord_mesh/req_protocol.h>

chord_mesh::ReqProtocolImpl::ReqProtocolImpl(
    StreamManager *manager,
    ReqProtocolErrorCallback error,
    ReqProtocolCleanupCallback cleanup)
    : m_manager(manager),
      m_error(error),
      m_cleanup(cleanup)
{
    TU_ASSERT (m_manager != nullptr);
    TU_ASSERT (m_error != nullptr);
    TU_ASSERT (m_cleanup != nullptr);
}

void
chord_mesh::on_stream_receive(const Message &message, void *data)
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
    stream->start(ops);
    while (!impl->m_pending.empty()) {
        auto pending = impl->m_pending.front();
        stream->send(MessageVersion::Version1, pending.second);
        impl->m_pending.pop();
    }
    impl->m_stream = std::move(stream);
}

void
chord_mesh::on_connect_error(const tempo_utils::Status &status, void *data)
{
    TU_LOG_ERROR << status;
}

tempo_utils::Status
chord_mesh::ReqProtocolImpl::connect(const chord_common::TransportLocation &location)
{
    StreamConnectorOps ops;
    ops.connect = on_connect_complete;
    ops.error = on_connect_error;
    StreamConnectorOptions options;
    options.data = this;

    TU_ASSIGN_OR_RETURN (m_connector, StreamConnector::create(m_manager, ops, options));
    TU_ASSIGN_OR_RETURN (m_connectId, m_connector->connectLocation(location));
    return {};
}

tempo_utils::Result<tu_uint32>
chord_mesh::ReqProtocolImpl::send(::capnp::MessageBuilder &builder)
{
    // serialize the payload
    auto flatArray = capnp::messageToFlatArray(builder);
    auto arrayPtr = flatArray.asBytes();
    std::span messageBytes(arrayPtr.begin(), arrayPtr.end());
    auto payload = tempo_utils::MemoryBytes::copy(messageBytes);

    auto id = m_currId++;
    if (m_stream != nullptr) {
        TU_RETURN_IF_NOT_OK (m_stream->send(MessageVersion::Version1, payload, absl::Now()));
    } else {
        m_pending.emplace(id, payload);
    }
    return id;
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