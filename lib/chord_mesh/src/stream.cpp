
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/noise.h>
#include <chord_mesh/stream.h>

chord_mesh::Stream::Stream(StreamHandle *handle)
    : m_handle(handle)
{
    TU_ASSERT (m_handle != nullptr);
}

chord_mesh::Stream::~Stream()
{
    if (m_handle != nullptr) {
        m_handle->shutdown();
        m_handle->release();
    }
}

bool
chord_mesh::Stream::isInitiator() const
{
    return m_handle->initiator;
}

bool
chord_mesh::Stream::isSecure() const
{
    return !m_handle->insecure;
}

tempo_utils::UUID
chord_mesh::Stream::getId() const
{
    return m_handle->id;
}

chord_mesh::StreamState
chord_mesh::Stream::getStreamState() const
{
    return m_handle->state;
}

tempo_utils::Status
chord_mesh::Stream::start(std::unique_ptr<AbstractStreamContext> &&ctx)
{
    return m_handle->start(std::move(ctx));
}

tempo_utils::Status
chord_mesh::Stream::negotiate(std::string_view protocolName)
{
    return m_handle->negotiate(protocolName);
}

tempo_utils::Status
chord_mesh::Stream::send(
    EnvelopeVersion version,
    std::shared_ptr<const tempo_utils::ImmutableBytes> payload,
    absl::Time timestamp)
{
    if (m_handle->state == StreamState::Closed)
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "stream is closed");
    EnvelopeBuilder builder;
    builder.setVersion(version);
    builder.setPayload(std::move(payload));
    builder.setTimestamp(timestamp);
    std::shared_ptr<const tempo_utils::ImmutableBytes> bytes;
    TU_ASSIGN_OR_RETURN (bytes, builder.toBytes());
    return m_handle->send(bytes);
}

void
chord_mesh::Stream::shutdown()
{
    m_handle->shutdown();
}

void
chord_mesh::Stream::close()
{
    m_handle->close();
}
