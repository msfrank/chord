#ifndef CHORD_AGENT_MACHINE_SUPERVISOR_H
#define CHORD_AGENT_MACHINE_SUPERVISOR_H

#include <filesystem>
#include <future>

#include <absl/container/flat_hash_map.h>
#include <grpcpp/grpcpp.h>
#include <uv.h>

#include <chord_agent/machine_process.h>
#include <chord_invoke/invoke_service.pb.h>
#include <lyric_common/assembly_location.h>
#include <tempo_utils/process_utils.h>
#include <tempo_utils/url.h>

#include "machine_logger.h"

// forward declarations
class MachineSupervisor;

struct MachineHandle {
    tempo_utils::Url url;
};

struct ExitStatus {
    tempo_utils::Url url;
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
    tempo_utils::Url url;
    uv_timer_t timeout;
    std::shared_ptr<OnSupervisorSpawn> waiter;
    MachineSupervisor *supervisor;
};

struct SigningContext {
    tempo_utils::Url url;
    uv_timer_t timeout;
    std::shared_ptr<OnSupervisorSign> waiter;
    MachineSupervisor *supervisor;
};

struct ReadyContext {
    tempo_utils::Url url;
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
        uv_loop_t *loop,
        const std::filesystem::path &processRunDirectory,
        int idleTimeoutSeconds,
        int registrationTimeoutSeconds);
    ~MachineSupervisor();

    tempo_utils::Status initialize();
    bool isIdle();

    uv_loop_t *getLoop() const;

    tempo_utils::Status spawnMachine(
        const tempo_utils::Url &machineUrl,
        const tempo_utils::ProcessInvoker &invoker,
        std::shared_ptr<OnSupervisorSpawn> waiter);

    tempo_utils::Status requestCertificates(
        const tempo_utils::Url &machineUrl,
        const chord_invoke::SignCertificatesRequest &signCertificatesRequest,
        std::shared_ptr<OnSupervisorSign> waiter);

    tempo_utils::Status bindCertificates(
        const tempo_utils::Url &machineUrl,
        const chord_invoke::RunMachineRequest &runMachineRequest,
        std::shared_ptr<OnSupervisorReady> waiter);

    tempo_utils::Status startMachine(
        const tempo_utils::Url &machineUrl,
        const chord_invoke::AdvertiseEndpointsRequest &advertiseEndpointsRequest);

    tempo_utils::Status terminateMachine(
        const tempo_utils::Url &machineUrl,
        std::shared_ptr<OnSupervisorTerminate> waiter);

    tempo_utils::Status shutdown();

private:
    uv_loop_t *m_loop;
    std::filesystem::path m_processRunDirectory;
    int m_idleTimeoutSeconds;
    int m_registrationTimeoutSeconds;
    absl::Mutex m_lock;
    uv_timer_t m_idle;
    absl::flat_hash_map<tempo_utils::Url, std::unique_ptr<MachineProcess>> m_machines;
    absl::flat_hash_map<tempo_utils::Url, std::unique_ptr<SpawningContext>> m_spawning;
    absl::flat_hash_map<tempo_utils::Url, std::unique_ptr<SigningContext>> m_signing;
    absl::flat_hash_map<tempo_utils::Url, std::unique_ptr<ReadyContext>> m_ready;
    absl::flat_hash_map<tempo_utils::Url, std::unique_ptr<WaitingContext>> m_waiting;
    bool m_shuttingDown;

    tempo_utils::Status release(const tempo_utils::Url &processUrl, tu_int64 status, int signal);
    tempo_utils::Status abandon(const tempo_utils::Url &processUrl);
    tempo_utils::Status reap(const tempo_utils::Url &processUrl);

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

#endif // CHORD_AGENT_MACHINE_SUPERVISOR_H