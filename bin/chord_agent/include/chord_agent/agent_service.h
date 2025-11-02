#ifndef CHORD_AGENT_AGENT_SERVICE_H
#define CHORD_AGENT_AGENT_SERVICE_H

#include <chord_invoke/invoke_service.grpc.pb.h>

#include "agent_config.h"
#include "machine_supervisor.h"

namespace chord_agent {

    class AgentService : public chord_invoke::InvokeService::CallbackService {
    public:
        AgentService(const AgentConfig &agentConfig, uv_loop_t *loop);

        tempo_utils::Status initialize(const chord_common::TransportLocation &supervisorEndpoint);
        tempo_utils::Status shutdown();

        grpc::ServerUnaryReactor *
        IdentifyAgent(
            grpc::CallbackServerContext *context,
            const chord_invoke::IdentifyAgentRequest *request,
            chord_invoke::IdentifyAgentResult *response) override;

        grpc::ServerUnaryReactor *
        CreateMachine(
            grpc::CallbackServerContext *context,
            const chord_invoke::CreateMachineRequest *request,
            chord_invoke::CreateMachineResult *response) override;

        grpc::ServerUnaryReactor *
        SignCertificates(
            grpc::CallbackServerContext *context,
            const chord_invoke::SignCertificatesRequest *request,
            chord_invoke::SignCertificatesResult *response) override;

        grpc::ServerUnaryReactor *
        RunMachine(
            grpc::CallbackServerContext *context,
            const chord_invoke::RunMachineRequest *request,
            chord_invoke::RunMachineResult *response) override;

        grpc::ServerUnaryReactor *
        AdvertiseEndpoints(
            grpc::CallbackServerContext *context,
            const chord_invoke::AdvertiseEndpointsRequest *request,
            chord_invoke::AdvertiseEndpointsResult *response) override;

        grpc::ServerUnaryReactor *
        DeleteMachine(
            grpc::CallbackServerContext *context,
            const chord_invoke::DeleteMachineRequest *request,
            chord_invoke::DeleteMachineResult *response) override;

    private:
        const AgentConfig &m_agentConfig;
        uv_loop_t *m_loop;
        tu_uint64 m_uptime;

        std::unique_ptr<absl::Mutex> m_lock;
        std::unique_ptr<MachineSupervisor> m_supervisor ABSL_GUARDED_BY(m_lock);

        tempo_utils::Status doCreateMachine(
            grpc::ServerUnaryReactor *reactor,
            grpc::CallbackServerContext *context,
            const chord_invoke::CreateMachineRequest *request,
            chord_invoke::CreateMachineResult *response);

        tempo_utils::Status doSignCertificates(
            grpc::ServerUnaryReactor *reactor,
            grpc::CallbackServerContext *context,
            const chord_invoke::SignCertificatesRequest *request,
            chord_invoke::SignCertificatesResult *response);

        tempo_utils::Status doRunMachine(
            grpc::ServerUnaryReactor *reactor,
            grpc::CallbackServerContext *context,
            const chord_invoke::RunMachineRequest *request,
            chord_invoke::RunMachineResult *response);

        tempo_utils::Status doAdvertiseEndpoints(
            grpc::ServerUnaryReactor *reactor,
            grpc::CallbackServerContext *context,
            const chord_invoke::AdvertiseEndpointsRequest *request,
            chord_invoke::AdvertiseEndpointsResult *response);

        tempo_utils::Status doDeleteMachine(
            grpc::ServerUnaryReactor *reactor,
            grpc::CallbackServerContext *context,
            const chord_invoke::DeleteMachineRequest *request,
            chord_invoke::DeleteMachineResult *response);
    };

    class OnAgentSpawn : public OnSupervisorSpawn {
    public:
        OnAgentSpawn(grpc::ServerUnaryReactor *reactor, chord_invoke::CreateMachineResult *result);
        void onComplete(
            MachineHandle handle,
            const chord_invoke::SignCertificatesRequest &signCertificatesRequest) override;
        void onStatus(tempo_utils::Status status) override;

    private:
        grpc::ServerUnaryReactor *m_reactor;
        chord_invoke::CreateMachineResult *m_result;
    };

    class OnAgentSign : public OnSupervisorSign {
    public:
        OnAgentSign(grpc::ServerUnaryReactor *reactor, chord_invoke::SignCertificatesResult *result);
        void onComplete(
            MachineHandle handle,
            const chord_invoke::RunMachineRequest &runMachineRequest) override;
        void onStatus(tempo_utils::Status status) override;

    private:
        grpc::ServerUnaryReactor *m_reactor;
        chord_invoke::SignCertificatesResult *m_result;
    };

    class OnAgentReady : public OnSupervisorReady {
    public:
        OnAgentReady(grpc::ServerUnaryReactor *reactor, chord_invoke::RunMachineResult *result);
        void onComplete(
            MachineHandle handle,
            const chord_invoke::AdvertiseEndpointsRequest &advertiseEndpointsRequest) override;
        void onStatus(tempo_utils::Status status) override;

    private:
        grpc::ServerUnaryReactor *m_reactor;
        chord_invoke::RunMachineResult *m_result;
    };

    class OnAgentTerminate : public OnSupervisorTerminate {
    public:
        OnAgentTerminate(grpc::ServerUnaryReactor *reactor, chord_invoke::DeleteMachineResult *result);
        void onComplete(ExitStatus exitStatus) override;
        void onStatus(tempo_utils::Status status) override;

    private:
        grpc::ServerUnaryReactor *m_reactor;
        chord_invoke::DeleteMachineResult *m_result;
    };
}

#endif // CHORD_AGENT_AGENT_SERVICE_H