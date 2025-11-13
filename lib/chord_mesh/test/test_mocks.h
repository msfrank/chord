#ifndef CHORD_MESH_TEST_MOCKS_H
#define CHORD_MESH_TEST_MOCKS_H

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_mesh/stream_buf.h>

class MockStreamBufWriter : public chord_mesh::AbstractStreamBufWriter {
public:
    MOCK_METHOD (
        tempo_utils::Status,
        write,
        (chord_mesh::StreamBuf *),
        (override));
};

#endif // CHORD_MESH_TEST_MOCKS_H