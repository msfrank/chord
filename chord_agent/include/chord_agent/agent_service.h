#ifndef CHORD_AGENT_AGENT_SERVICE_H
#define CHORD_AGENT_AGENT_SERVICE_H

#include <boost/uuid/random_generator.hpp>

#include <chord_invoke/invoke_service.grpc.pb.h>

#include "machine_supervisor.h"

class AgentService : public chord_invoke::InvokeService::CallbackService {

public:
    AgentService(
        const std::string &listenEndpoint,
        MachineSupervisor *supervisor,
        std::string_view agentName,
        const std::filesystem::path &localMachineExecutable);

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
    std::string m_listenEndpoint;
    MachineSupervisor *m_supervisor;
    std::string m_agentName;
    std::filesystem::path m_localMachineExecutable;
    tu_uint64 m_uptime;
    boost::uuids::random_generator m_uuidgen;
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

#endif // CHORD_AGENT_AGENT_SERVICE_H