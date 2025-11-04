
#include "base_mesh_fixture.h"

void stop_loop(uv_async_t *async)
{
    uv_stop(async->loop);
}

void BaseMeshFixture::SetUp()
{
    uv_loop_init(&m_loop);
    uv_async_init(&m_loop, &m_async, stop_loop);
}

uv_loop_t *
BaseMeshFixture::getUVLoop()
{
    return &m_loop;
}

void run_thread(void *arg)
{
    auto *loop = (uv_loop_t *) arg;
    uv_run(loop, UV_RUN_DEFAULT);
}

tempo_utils::Status
BaseMeshFixture::startUVThread()
{
    m_tid = std::make_unique<uv_thread_t>();
    uv_thread_create(m_tid.get(), run_thread, &m_loop);
    return {};
}

tempo_utils::Status
BaseMeshFixture::stopUVThread()
{
    uv_async_send(&m_async);
    uv_thread_join(m_tid.get());
    m_tid.reset();
    return {};
}
