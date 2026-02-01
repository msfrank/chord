
#include <chord_mesh/connect.h>

chord_mesh::Connect::Connect(ConnectHandle *handle)
    : m_handle(handle),
      m_id(tempo_utils::UUID::randomUUID()),
      m_state(ConnectState::Pending)
{
    TU_ASSERT (m_handle != nullptr);
}

chord_mesh::Connect::~Connect()
{
    m_handle->release();
}

tempo_utils::UUID
chord_mesh::Connect::getId() const
{
    return m_handle->id;
}

chord_mesh::ConnectState
chord_mesh::Connect::getConnectState() const
{
    return m_handle->state;
}

void
chord_mesh::Connect::abort()
{
    m_handle->abort();
}