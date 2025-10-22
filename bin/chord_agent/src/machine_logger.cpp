
#include <chord_agent/machine_logger.h>
#include <tempo_utils/log_stream.h>

chord_agent::MachineLogger::MachineLogger(const tempo_utils::Url &machineUrl, uv_loop_t *loop)
    : m_machineUrl(machineUrl),
      m_loop(loop)
{
    TU_ASSERT (m_machineUrl.isValid());
    TU_ASSERT (m_loop != nullptr);
    memset(&m_out, 0, sizeof(uv_pipe_t));
    memset(&m_err, 0, sizeof(uv_pipe_t));
    m_outIsClosed = true;
    m_errIsClosed = true;
}

chord_agent::MachineLogger::~MachineLogger()
{
    closeLoggerUnconditionally();
}

tempo_utils::Status
chord_agent::MachineLogger::initialize()
{
    if (m_loop == nullptr)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation, "initialization failed");

    int ret;

    ret = uv_pipe_init(m_loop, &m_out, 0);
    if (ret < 0) {
        m_loop = nullptr;   // set null so initialization cannot be invoked more than once
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation,
            "failed to create output pipe: {} ({})", uv_strerror(ret), uv_err_name(ret));
    }
    m_out.data = this;
    m_outIsClosed = false;

    ret = uv_pipe_init(m_loop, &m_err, 0);
    if (ret < 0) {
        m_loop = nullptr;   // set null so initialization cannot be invoked more than once
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation,
            "failed to create error pipe: {} ({})", uv_strerror(ret), uv_err_name(ret));
    }
    m_err.data = this;
    m_errIsClosed = false;

    return {};
}

static void
on_buf_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t *buf)
{
    buf->base = (char *) malloc(suggested_size);
    TU_ASSERT (buf->base != nullptr);
    buf->len = suggested_size;
}

void
chord_agent::on_pipe_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    auto *logger = (chord_agent::MachineLogger *) stream->data;

    // empty read, nothing to do
    if (nread == 0)
        return;

    // if we reached the end of the stream, then close it
    if (nread == UV_EOF) {
        logger->closeLogger(stream);
        return;
    }

    // otherwise if nread indicates any other error then log it
    if (nread < 0) {
        TU_LOG_ERROR << "failed to read from stream: " << uv_strerror(nread)
            << "(" << uv_err_name(nread) << ")";
        return;
    }

    std::string s(buf->base, nread);

    // log the contents
    if (stream == (uv_stream_t *) &logger->m_err) {
        TU_LOG_INFO << "machine " << logger->m_machineUrl << " ERR ctx " << stream->data << ": " << s;
    } else if (stream == (uv_stream_t *) &logger->m_out) {
        TU_LOG_INFO << "machine " << logger->m_machineUrl << " OUT ctx " << stream->data << ": " << s;
    } else {
        TU_LOG_INFO << "machine " << logger->m_machineUrl << " UNKNOWN ctx " << stream->data << ": " << s;
    }
}

tempo_utils::Status
chord_agent::MachineLogger::openLogger()
{
    int ret;

    ret = uv_read_start((uv_stream_t *) &m_out, on_buf_alloc, on_pipe_read);
    if (ret < 0)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation,
            "failed to start read on output pipe: {} ({})", uv_strerror(ret), uv_err_name(ret));

    ret = uv_read_start((uv_stream_t *) &m_err, on_buf_alloc, on_pipe_read);
    if (ret < 0)
        return tempo_utils::GenericStatus::forCondition(
            tempo_utils::GenericCondition::kInternalViolation,
            "failed to start read on error pipe: {} ({})", uv_strerror(ret), uv_err_name(ret));

    return {};
}

tempo_utils::Status
chord_agent::MachineLogger::closeLogger(uv_stream_t *stream)
{
    TU_ASSERT (stream != nullptr);

    if (stream == (uv_stream_t *) &m_out) {
        uv_close((uv_handle_t *) &m_out, nullptr);
        m_outIsClosed = true;
    }
    if (stream == (uv_stream_t *) &m_err) {
        uv_close((uv_handle_t *) &m_err, nullptr);
        m_errIsClosed = true;
    }
    return {};
}

tempo_utils::Status
chord_agent::MachineLogger::closeLoggerUnconditionally()
{
    if (!m_outIsClosed) {
        uv_close((uv_handle_t *) &m_out, nullptr);
    }
    if (!m_errIsClosed) {
        uv_close((uv_handle_t *) &m_err, nullptr);
    }
    return {};
}

uv_stream_t *
chord_agent::MachineLogger::getOutput() const
{
    return (uv_stream_t *) &m_out;
}

uv_stream_t *
chord_agent::MachineLogger::getError() const
{
    return (uv_stream_t *) &m_err;
}
