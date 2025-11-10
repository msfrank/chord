
#include <chord_mesh/stream_manager.h>

#include "chord_mesh/mesh_result.h"

chord_mesh::StreamManager::StreamManager(
    uv_loop_t *loop,
    std::shared_ptr<tempo_security::X509Store> trustStore,
    const StreamManagerOps &ops,
    void *data)
    : m_loop(loop),
      m_trustStore(std::move(trustStore)),
      m_ops(ops),
      m_data(data),
      m_handles(nullptr),
      m_running(true)
{
    TU_ASSERT (m_loop != nullptr);
    TU_ASSERT (m_trustStore != nullptr);
}

uv_loop_t *
chord_mesh::StreamManager::getLoop() const
{
    return m_loop;
}

std::shared_ptr<tempo_security::X509Store>
chord_mesh::StreamManager::getTrustStore() const
{
    return m_trustStore;
}

chord_mesh::StreamHandle *
chord_mesh::StreamManager::allocateHandle(uv_stream_t *stream, void *data)
{
    TU_ASSERT (stream != nullptr);

    if (!m_running)
        return nullptr;

    auto *handle = new StreamHandle(stream, this, data);
    stream->data = handle;
    handle->stream = stream;
    handle->manager = this;

    // case 1: there are no active handles
    if (m_handles == nullptr) {
        m_handles = handle;
        handle->prev = nullptr;
        handle->next = nullptr;
        return handle;
    }

    auto *next = m_handles->next;
    handle->next = m_handles;

    // case 2: there is a single active handle
    if (next == nullptr) {
        TU_ASSERT (m_handles->prev == nullptr);
        handle->prev = m_handles;
        m_handles->prev = handle;
        m_handles->next = handle;
        return handle;
    }

    // case 3: there is more than 1 active handle
    auto *prev = m_handles->prev;
    handle->prev = prev;
    next->prev = handle;
    prev->next = handle;
    return handle;
}

void
chord_mesh::StreamManager::freeHandle(StreamHandle *handle)
{
    TU_ASSERT (handle != nullptr);

    // case 1: the handle has no siblings
    if (handle->next == nullptr) {
        TU_ASSERT (handle->prev == nullptr);
        TU_ASSERT (m_handles == handle);
        delete handle;
        m_handles = nullptr;
        return;
    }

    TU_ASSERT (handle->prev != nullptr);
    auto *prev = handle->prev;
    auto *next = handle->next;

    // case 2: the handle has one sibling
    if (prev == next) {
        if (m_handles == handle) {
            m_handles = next;
        }
        m_handles->prev = nullptr;
        m_handles->next = nullptr;
        return;
    }

    // case 3: the handle has more than sibling
    if (m_handles == handle) {
        m_handles = next;
    }
    prev->next = next;
    next->prev = prev;
}

void
chord_mesh::StreamManager::shutdown()
{
    if (!m_running)
        return;
    m_running = false;
}

void
chord_mesh::StreamManager::emitError(const tempo_utils::Status &status)
{
    if (m_ops.error != nullptr) {
        m_ops.error(status, m_data);
    }
}

chord_mesh::StreamHandle::StreamHandle(uv_stream_t *stream, StreamManager *manager, void *data)
    : stream(stream),
      manager(manager),
      data(data),
      prev(nullptr),
      next(nullptr),
      m_closing(false)
{
    TU_ASSERT (stream != nullptr);
    TU_ASSERT (manager != nullptr);
}

void
chord_mesh::close_stream(uv_handle_t *stream)
{
    auto *handle = (StreamHandle *) stream->data;
    auto *manager = handle->manager;
    manager->freeHandle(handle);
}

void
chord_mesh::shutdown_stream(uv_shutdown_t *req, int err)
{
    auto *handle = (StreamHandle *) req->handle->data;
    auto *manager = handle->manager;
    if (err < 0) {
        manager->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_shutdown error: {}", uv_strerror(err)));
    }
    uv_close((uv_handle_t *) req->handle, close_stream);
}

void
chord_mesh::StreamHandle::shutdown()
{
    if (m_closing)
        return;
    memset(&m_req, 0, sizeof(uv_shutdown_t));
    auto ret = uv_shutdown(&m_req, stream, shutdown_stream);
    if (ret < 0) {
        manager->emitError(
            MeshStatus::forCondition(MeshCondition::kMeshInvariant,
                "uv_shutdown error: {}", uv_strerror(ret)));
        return;
    }

    m_closing = true;
}

void
chord_mesh::StreamHandle::close()
{
    if (m_closing)
        return;
    uv_close((uv_handle_t *) stream, close_stream);
    m_closing = true;
}
