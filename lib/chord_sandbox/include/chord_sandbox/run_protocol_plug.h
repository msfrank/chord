#ifndef CHORD_SANDBOX_RUN_PROTOCOL_PLUG_H
#define CHORD_SANDBOX_RUN_PROTOCOL_PLUG_H

#include <absl/synchronization/mutex.h>
#include <uv.h>

#include <chord_common/abstract_protocol_handler.h>
#include <tempo_utils/result.h>

namespace chord_sandbox {

    constexpr char const *kRunProtocolUri = "dev.zuri.proto:run";

    enum class RunPlugState {
        INVALID,
        READY,
        STARTING,
        RUNNING,
        STOPPING,
        STOPPED,
        FINISHED,
    };

    typedef void (*RunProtocolCallback)(RunPlugState, void *);

class RunProtocolPlug : public chord_common::AbstractProtocolHandler {

    public:
        RunProtocolPlug(RunProtocolCallback cb, void *data);

        bool isAttached() override;
        tempo_utils::Status attach(chord_common::AbstractProtocolWriter *writer) override;
        tempo_utils::Status send(std::string_view message) override;
        tempo_utils::Status handle(std::string_view message) override;
        tempo_utils::Status detach() override;

        RunPlugState getState();

        tempo_utils::Result<RunPlugState> waitForStateChange(RunPlugState prev, int timeout = -1);

    private:
        RunProtocolCallback m_cb;
        void *m_data;
        absl::Mutex m_lock;
        RunPlugState m_state ABSL_GUARDED_BY(m_lock);
        chord_common::AbstractProtocolWriter *m_writer ABSL_GUARDED_BY(m_lock);
        absl::CondVar m_cond;
    };
}

#endif // CHORD_SANDBOX_RUN_PROTOCOL_PLUG_H