#ifndef CHORD_SANDBOX_REMOTE_MACHINE_H
#define CHORD_SANDBOX_REMOTE_MACHINE_H

#include <chord_sandbox/grpc_connector.h>
#include <lyric_common/assembly_location.h>
#include <tempo_utils/url.h>

namespace chord_sandbox {

    /**
     *
     */
    typedef void (*RemoteMachineStateChangedFunc)(chord_remoting::MachineState, void *);

    class RemoteMachine {

    public:
        RemoteMachine(
            std::string_view name,
            const lyric_common::AssemblyLocation &mainLocation,
            const tempo_utils::Url &machineUrl,
            std::shared_ptr<GrpcConnector> connector);

        tempo_utils::Status runUntilFinished(RemoteMachineStateChangedFunc func = nullptr, void *data = nullptr);
        tempo_utils::Status suspend();
        tempo_utils::Status resume();
        tempo_utils::Status shutdown();

    private:
        std::string m_name;
        lyric_common::AssemblyLocation m_mainLocation;
        tempo_utils::Url m_machineUrl;
        std::shared_ptr<GrpcConnector> m_connector;
    };
}


#endif // CHORD_SANDBOX_REMOTE_MACHINE_H