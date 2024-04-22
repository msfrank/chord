#ifndef CHORD_SANDBOX_REMOTE_MACHINE_H
#define CHORD_SANDBOX_REMOTE_MACHINE_H

#include <chord_protocol/abstract_protocol_handler.h>
#include <chord_sandbox/grpc_connector.h>
#include <chord_sandbox/run_protocol_plug.h>
#include <lyric_common/assembly_location.h>
#include <tempo_config/config_types.h>
#include <tempo_utils/url.h>

namespace chord_sandbox {

    class RemoteMachine {

    public:
        RemoteMachine(
            std::string_view name,
            const lyric_common::AssemblyLocation &mainLocation,
            const tempo_utils::Url &machineUrl,
            std::shared_ptr<GrpcConnector> connector,
            std::shared_ptr<RunProtocolPlug> runPlug);

        tempo_utils::Status runUntilFinished();
        tempo_utils::Status start();
        tempo_utils::Status stop();
        tempo_utils::Status shutdown();

    private:
        std::string m_name;
        lyric_common::AssemblyLocation m_mainLocation;
        tempo_utils::Url m_machineUrl;
        std::shared_ptr<GrpcConnector> m_connector;
        std::shared_ptr<RunProtocolPlug> m_runPlug;
        absl::Mutex m_lock;
    };
}


#endif // CHORD_SANDBOX_REMOTE_MACHINE_H