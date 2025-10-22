#ifndef CHORD_AGENT_MACHINE_LOGGER_H
#define CHORD_AGENT_MACHINE_LOGGER_H

#include <uv.h>

#include <tempo_utils/status.h>
#include <tempo_utils/url.h>

namespace chord_agent {

    class MachineLogger {
    public:
        MachineLogger(const tempo_utils::Url &machineUrl, uv_loop_t *loop);
        ~MachineLogger();

        tempo_utils::Status initialize();

        tempo_utils::Status openLogger();
        tempo_utils::Status closeLogger(uv_stream_t *stream);
        tempo_utils::Status closeLoggerUnconditionally();

        uv_stream_t *getOutput() const;
        uv_stream_t *getError() const;

    private:
        tempo_utils::Url m_machineUrl;
        uv_loop_t *m_loop;
        uv_pipe_t m_out;
        uv_pipe_t m_err;
        bool m_outIsClosed;
        bool m_errIsClosed;

        friend void on_pipe_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
    };
}

#endif // CHORD_AGENT_MACHINE_LOGGER_H