#ifndef CHORD_AGENT_MACHINE_PROCESS_H
#define CHORD_AGENT_MACHINE_PROCESS_H

#include <uv.h>

#include <chord_common/common_conversions.h>
#include <chord_common/common_types.h>
#include <tempo_utils/process_builder.h>
#include <tempo_utils/status.h>
#include <tempo_utils/url.h>
#include <absl/synchronization/mutex.h>
#include <zuri_packager/package_reader.h>

#include "machine_logger.h"

namespace chord_agent {

    class MachineSupervisor;

    enum class MachineState {
        Initial,                    // initial state, process has not been spawned
        Created,                    // process has been spawned but has not started initialization
        Starting,                   // process is performing initialization
        Running,                    // process has completed initialization and is ready to accept protocol connections
        Terminating,                // process has been signaled to terminate and is shutting down
        Exited,                     // process has finished, exit status and termination signal are available
    };

    struct MachineOptions {
        std::filesystem::path machineExecutable = {};
        std::filesystem::path runDirectory = {};
        std::vector<std::filesystem::path> packageCacheDirectories = {};
        std::filesystem::path pemRootCABundleFile = {};
        std::vector<chord_common::RequestedPort> requestedPorts = {};
        std::vector<std::string> mainArguments = {};
        bool enableMonitoring = false;
        bool startSuspended = false;
    };

    class MachineProcess {
    public:
        static tempo_utils::Result<std::shared_ptr<MachineProcess>> create(
            std::string_view machineName,
            const zuri_packager::PackageSpecifier &mainPackage,
            const chord_common::TransportLocation &supervisorEndpoint,
            MachineSupervisor *supervisor,
            const MachineOptions &options = {});
        virtual ~MachineProcess();

        std::string getMachineName() const;

        MachineState getState() const;
        void setState(MachineState state);

        tempo_utils::Status spawn(const std::filesystem::path &runDirectory = {});
        tempo_utils::Status terminate(int signal = -1);

    private:
        std::string m_machineName;
        tempo_utils::ProcessInvoker m_invoker;
        MachineSupervisor *m_supervisor;
        absl::Mutex *m_lock;
        uv_process_t m_process;
        MachineState m_state;
        std::unique_ptr<MachineLogger> m_logger;
        tu_int64 m_exitStatus;
        int m_exitSignal;

        MachineProcess(
            const std::string &machineName,
            const tempo_utils::ProcessInvoker &invoker,
            MachineSupervisor *supervisor);
        void release(tu_int64 status, int signal);

        friend void on_process_exit(uv_process_t *child, int64_t status, int signal);
    };
}

#endif // CHORD_AGENT_MACHINE_PROCESS_H
