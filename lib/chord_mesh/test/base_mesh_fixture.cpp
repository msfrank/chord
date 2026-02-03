
#include "base_mesh_fixture.h"

void stop_loop(uv_async_t *async)
{
    uv_stop(async->loop);
}

void BaseMeshFixture::SetUp()
{
    tempo_utils::LoggingConfiguration loggingConfig;
    loggingConfig.severityFilter = tempo_utils::SeverityFilter::kVerbose;
    tempo_utils::init_logging(loggingConfig);

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

chord_mesh::Envelope
parse_raw_envelope(std::span<const tu_uint8> raw)
{
    chord_mesh::EnvelopeParser parser;
    parser.pushBytes(raw);
    bool ready;
    TU_RAISE_IF_NOT_OK (parser.checkReady(ready));
    chord_mesh::Envelope message;
    TU_RAISE_IF_NOT_OK (parser.takeReady(message));
    return message;
}

ssize_t
read_until_eof(int fd, std::vector<tu_uint8> &buf)
{
    TU_ASSERT (fd >= 0);

    tu_uint8 buffer[128];
    ssize_t ret;
    do {
        ret = read(fd, buffer, 128);
        if (ret < 0)
            return ret;
        buf.insert(buf.end(), buffer, buffer + ret);
    } while (ret > 0);

    return static_cast<ssize_t>(buf.size());
}

bool
write_entire_buffer(int fd, std::span<const tu_uint8> buf)
{
    auto *data = buf.data();
    auto nleft = buf.size();

    while (nleft > 0) {
        auto ret = write(fd, data, nleft);
        if (ret <= 0)
            return false;
        data += ret;
        nleft -= ret;
    }
    return true;
}
