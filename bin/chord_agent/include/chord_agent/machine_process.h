#ifndef CHORD_AGENT_MACHINE_PROCESS_H
#define CHORD_AGENT_MACHINE_PROCESS_H

#include <uv.h>

#include <chord_agent/machine_logger.h>
#include <tempo_utils/process_builder.h>
#include <tempo_utils/status.h>
#include <tempo_utils/url.h>
#include <absl/synchronization/mutex.h>

class MachineSupervisor;

enum class MachineState {
    Initial,                    // initial state, process has not been spawned
    Created,                    // process has been spawned but has not started initialization
    Starting,                   // process is performing initialization
    Running,                    // process has completed initialization and is ready to accept protocol connections
    Terminating,                // process has been signaled to terminate and is shutting down
    Exited,                     // process has finished, exit status and termination signal are available
};

class MachineProcess {
public:
    MachineProcess(
        const tempo_utils::Url &machineUrl,
        const tempo_utils::ProcessInvoker &invoker,
        MachineSupervisor *supervisor);
    ~MachineProcess();

    tempo_utils::Url getMachineUrl() const;

    MachineState getState() const;
    void setState(MachineState state);

    tempo_utils::Status spawn(const std::filesystem::path &cwd);
    tempo_utils::Status terminate(int signal = -1);

private:
    tempo_utils::Url m_machineUrl;
    tempo_utils::ProcessInvoker m_invoker;
    MachineSupervisor *m_supervisor;
    absl::Mutex *m_lock;
    uv_process_t m_process;
    MachineState m_state;
    std::unique_ptr<MachineLogger> m_logger;
    tu_int64 m_exitStatus;
    int m_exitSignal;

    void release(tu_int64 status, int signal);

    friend void on_process_exit(uv_process_t *child, int64_t status, int signal);
};

#endif // CHORD_AGENT_MACHINE_PROCESS_H
