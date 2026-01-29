#ifndef CHORD_MESH_BASE_MESH_FIXTURE_H
#define CHORD_MESH_BASE_MESH_FIXTURE_H

#include <uv.h>
#include <gtest/gtest.h>

#include <chord_mesh/envelope.h>
#include <tempo_utils/status.h>

class BaseMeshFixture : public ::testing::Test {
protected:
    void SetUp() override;

    uv_loop_t *getUVLoop();
    tempo_utils::Status startUVThread();
    tempo_utils::Status stopUVThread();

private:
    uv_loop_t m_loop;
    uv_async_t m_async;
    std::unique_ptr<uv_thread_t> m_tid;
};

chord_mesh::Envelope parse_raw_envelope(std::span<const tu_uint8> raw);

#endif // CHORD_MESH_BASE_MESH_FIXTURE_H