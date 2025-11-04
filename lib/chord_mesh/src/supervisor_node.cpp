
#include <chord_mesh/supervisor_node.h>

#include "chord_mesh/mesh_result.h"
#include "chord_mesh/stream_connector.h"

chord_mesh::SupervisorNode::SupervisorNode(
    const std::string &ensembleEventsUri,
    uv_loop_t *loop)
    : m_ensembleEventsUri(ensembleEventsUri),
      m_loop(loop)
{
    TU_ASSERT (!m_ensembleEventsUri.empty());
    TU_ASSERT (m_loop != nullptr);
}

tempo_utils::Result<std::shared_ptr<chord_mesh::SupervisorNode>>
chord_mesh::SupervisorNode::create(
    const std::string &ensembleEventsUri,
    uv_loop_t *loop,
    const SupervisorNodeOptions &options)
{
    auto supervisorNode = std::shared_ptr<SupervisorNode>(new SupervisorNode(ensembleEventsUri, loop));
    return supervisorNode;
}

tempo_utils::Status
chord_mesh::SupervisorNode::configure()
{
    // nng_socket ensembleEvents;
    // int ret;
    //
    // ret = nng_bus0_open(&ensembleEvents);
    // if (ret != 0)
    //     return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
    //         "failed to open BUS socket: {}", nng_strerror((nng_err) ret));
    //
    // ret= nng_listen(ensembleEvents, m_ensembleEventsUri.c_str(), nullptr, 0);
    // if (ret != 0)
    //     return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
    //         "failed to listen on socket: {}", nng_strerror((nng_err) ret));
    //
    // SocketPollerOps ops;
    // ops.sendmsg = ensemble_events_sendmsg;
    // ops.recvmsg = ensemble_events_recvmsg;
    // ops.cleanup = nullptr;
    //
    // auto poller = std::make_unique<SocketPoller>(ensembleEvents, ops, m_loop);
    // TU_RETURN_IF_NOT_OK (poller->configure(this));
    // m_ensembleEvents = std::move(poller);

    return {};
}