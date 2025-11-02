#ifndef CHORD_AGENT_MACHINE_SUPERVISOR_H
#define CHORD_AGENT_MACHINE_SUPERVISOR_H

#include <filesystem>
#include <future>

#include <absl/container/flat_hash_map.h>
#include <grpcpp/grpcpp.h>
#include <uv.h>

#include <chord_invoke/invoke_service.pb.h>
#include <tempo_utils/process_builder.h>
#include <tempo_utils/url.h>

#include "agent_config.h"
#include "machine_logger.h"
#include "machine_process.h"

namespace chord_agent {

    // forward declarations
    class MachineSupervisor;

    struct MachineHandle {
        std::string machineName;
    };

    struct ExitStatus {
        std::string machineName;
        tu_int64 status;
        int signal;
    };

    class OnSupervisorSpawn {
    public:
        virtual ~OnSupervisorSpawn() = default;
        virtual void onComplete(
            MachineHandle handle,
            const chord_invoke::SignCertificatesRequest &signCertificatesRequest) = 0;
        virtual void onStatus(tempo_utils::Status status) = 0;
    };

    class OnSupervisorSign {
    public:
        virtual ~OnSupervisorSign() = default;
        virtual void onComplete(
            MachineHandle handle,
            const chord_invoke::RunMachineRequest &runMachineRequest) = 0;
        virtual void onStatus(tempo_utils::Status status) = 0;
    };

    class OnSupervisorReady {
    public:
        virtual ~OnSupervisorReady() = default;
        virtual void onComplete(
            MachineHandle handle,
            const chord_invoke::AdvertiseEndpointsRequest &advertiseEndpointsRequest) = 0;
        virtual void onStatus(tempo_utils::Status status) = 0;
    };

    class OnSupervisorTerminate {
    public:
        virtual ~OnSupervisorTerminate() = default;
        virtual void onComplete(ExitStatus exitStatus) = 0;
        virtual void onStatus(tempo_utils::Status status) = 0;
    };

    struct SpawningContext {
        std::string machineName;
        uv_timer_t timeout;
        std::shared_ptr<OnSupervisorSpawn> waiter;
        MachineSupervisor *supervisor;
    };

    struct SigningContext {
        std::string machineName;
        uv_timer_t timeout;
        std::shared_ptr<OnSupervisorSign> waiter;
        MachineSupervisor *supervisor;
    };

    struct ReadyContext {
        std::string machineName;
        uv_timer_t timeout;
        std::shared_ptr<OnSupervisorReady> waiter;
        MachineSupervisor *supervisor;
    };

    struct WaitingContext {
        std::shared_ptr<OnSupervisorTerminate> waiter;
    };

    class MachineSupervisor {
    public:
        MachineSupervisor(
            const AgentConfig &agentConfig,
            const chord_common::TransportLocation &supervisorEndpoint,
            uv_loop_t *loop);
        ~MachineSupervisor();

        tempo_utils::Status initialize();
        bool isIdle();

        uv_loop_t *getLoop() const;

        tempo_utils::Status spawnMachine(
            std::string_view machineName,
            const zuri_packager::PackageSpecifier &mainPackage,
            const MachineOptions &options,
            std::shared_ptr<OnSupervisorSpawn> waiter);

        tempo_utils::Status requestCertificates(
            std::string_view machineName,
            const chord_invoke::SignCertificatesRequest &signCertificatesRequest,
            std::shared_ptr<OnSupervisorSign> waiter);

        tempo_utils::Status bindCertificates(
            std::string_view machineName,
            const chord_invoke::RunMachineRequest &runMachineRequest,
            std::shared_ptr<OnSupervisorReady> waiter);

        tempo_utils::Status startMachine(
            std::string_view machineName,
            const chord_invoke::AdvertiseEndpointsRequest &advertiseEndpointsRequest);

        tempo_utils::Status terminateMachine(
            std::string_view machineName,
            std::shared_ptr<OnSupervisorTerminate> waiter);

        tempo_utils::Status shutdown();

    private:
        const AgentConfig &m_agentConfig;
        chord_common::TransportLocation m_supervisorEndpoint;
        uv_loop_t *m_loop;
        uv_timer_t m_idle;

        absl::Mutex m_lock;
        absl::flat_hash_map<std::string, std::shared_ptr<MachineProcess>> m_machines;
        absl::flat_hash_map<std::string, std::unique_ptr<SpawningContext>> m_spawning;
        absl::flat_hash_map<std::string, std::unique_ptr<SigningContext>> m_signing;
        absl::flat_hash_map<std::string, std::unique_ptr<ReadyContext>> m_ready;
        absl::flat_hash_map<std::string, std::unique_ptr<WaitingContext>> m_waiting;
        bool m_shuttingDown;

        tempo_utils::Status release(std::string_view processName, tu_int64 status, int signal);
        tempo_utils::Status abandon(std::string_view processName);
        tempo_utils::Status reap(std::string_view processName);

        friend class MachineProcess;
        friend void on_spawning_timeout(uv_timer_t *timer);
        friend void on_signing_timeout(uv_timer_t *timer);
        friend void on_ready_timeout(uv_timer_t *timer);
    };

    class OnInternalTerminate : public OnSupervisorTerminate {
    public:
        OnInternalTerminate();
        void onComplete(ExitStatus exitStatus) override;
        void onStatus(tempo_utils::Status status) override;
    };
}

#endif // CHORD_AGENT_MACHINE_SUPERVISOR_H