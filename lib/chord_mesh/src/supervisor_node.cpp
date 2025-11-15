
#include <capnp/message.h>
#include <capnp/serialize.h>

#include <chord_mesh/generated/ensemble_messages.capnp.h>
#include <chord_mesh/generated/stream_messages.capnp.h>
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/stream_connector.h>
#include <chord_mesh/supervisor_node.h>

chord_mesh::SupervisorNode::SupervisorNode(
    const chord_common::TransportLocation &supervisorEndpoint,
    const tempo_security::CertificateKeyPair &keypair,
    StreamManager *manager,
    std::shared_ptr<StreamAcceptor> acceptor,
    const SupervisorNodeOptions &options)
    : m_supervisorEndpoint(supervisorEndpoint),
      m_keypair(keypair),
      m_manager(manager),
      m_options(options),
      m_acceptor(std::move(acceptor))
{
    TU_ASSERT (!m_supervisorEndpoint.isValid());
    TU_ASSERT (!m_keypair.isValid());
    TU_ASSERT (m_manager != nullptr);
    TU_ASSERT (m_acceptor != nullptr);
}

chord_mesh::SupervisorNode::~SupervisorNode()
{
    shutdown();
}

tempo_utils::Result<std::shared_ptr<chord_mesh::SupervisorNode>>
chord_mesh::SupervisorNode::create(
    const chord_common::TransportLocation &supervisorEndpoint,
    const tempo_security::CertificateKeyPair &keypair,
    StreamManager *manager,
    const SupervisorNodeOptions &options)
{
    std::shared_ptr<StreamAcceptor> acceptor;
    TU_ASSIGN_OR_RETURN (acceptor, StreamAcceptor::forLocation(supervisorEndpoint, manager));
    auto supervisorNode = std::shared_ptr<SupervisorNode>(new SupervisorNode(
        supervisorEndpoint, keypair, manager, acceptor, options));
    return supervisorNode;
}

void
chord_mesh::on_supervisor_stream_receive(const Message &message, void *data)
{
    auto *peer = (SupervisorPeer *) data;
    //auto *supervisor = peer->supervisor;

    auto payload = message.getPayload();
    auto arrayPtr = kj::arrayPtr(payload->getData(), payload->getSize());
    kj::ArrayInputStream inputStream(arrayPtr);
    capnp::MallocMessageBuilder builder;
    capnp::readMessageCopy(inputStream, builder);

    auto root = builder.getRoot<generated::EnsembleMessage>();
    switch (root.getMessage().which()) {

        case generated::EnsembleMessage::Message::NODE_JOINED:
        case generated::EnsembleMessage::Message::NODE_LEFT:
        case generated::EnsembleMessage::Message::PORT_OPENED:
        case generated::EnsembleMessage::Message::PORT_CLOSED:
        default:
            break;
    }

    peer->lastMessageAt = absl::Now();
}

void
chord_mesh::on_supervisor_connector_connect(std::shared_ptr<Stream> stream, void *data)
{
    auto *connecting = (ConnectingPeer *) data;
    auto *supervisor = connecting->supervisor;

    // create a new peer entry
    auto peer = std::make_shared<SupervisorPeer>();
    peer->stream = stream;
    peer->supervisor = supervisor;
    peer->connectedAt = absl::Now();

    // start the stream
    StreamOps streamOps;
    streamOps.receive = on_supervisor_stream_receive;
    stream->start(streamOps, peer.get());

    // remove the connecting entry
    supervisor->m_connecting.erase(connecting->endpoint);

    // add peer entry to the peers map
    auto id = stream->getId();
    supervisor->m_peers[id] = std::move(peer);

    // read the certificate
    std::shared_ptr<tempo_security::X509Certificate> certificate;
    auto keypair = supervisor->m_keypair;
    TU_ASSIGN_OR_RAISE (certificate, tempo_security::X509Certificate::readFile(keypair.getPemCertificateFile()));

    // construct the PeerHello message
    ::capnp::MallocMessageBuilder message;
    generated::EnsembleMessage::Builder root = message.initRoot<generated::EnsembleMessage>();
    auto peerHello = root.initMessage().initPeerHello();
    peerHello.setCertificate(certificate->toString());

    // send the message
    auto flatArray = capnp::messageToFlatArray(message);
    auto arrayPtr = flatArray.asBytes();
    auto payload = tempo_utils::MemoryBytes::copy(
        std::span(arrayPtr.begin(), arrayPtr.end()));

    TU_RAISE_IF_NOT_OK (stream->send(MessageVersion::Version1, payload));
}

void
chord_mesh::on_supervisor_connector_error(const tempo_utils::Status &status, void *data)
{
    TU_LOG_WARN << "connector error: " << status;
}

void
chord_mesh::on_supervisor_connector_cleanup(void *data)
{
}

void
chord_mesh::on_supervisor_acceptor_accept(std::shared_ptr<Stream> stream, void *data)
{
    auto *supervisor = (SupervisorNode *) data;

    // create a new peer entry
    auto peer = std::make_shared<SupervisorPeer>();
    peer->stream = stream;
    peer->supervisor = supervisor;
    peer->connectedAt = absl::Now();

    // start the stream
    StreamOps streamOps;
    streamOps.receive = on_supervisor_stream_receive;
    stream->start(streamOps, peer.get());

    // add peer entry to the peers map
    auto id = stream->getId();
    supervisor->m_peers[id] = std::move(peer);

    // read the certificate
    std::shared_ptr<tempo_security::X509Certificate> certificate;
    auto keypair = supervisor->m_keypair;
    TU_ASSIGN_OR_RAISE (certificate, tempo_security::X509Certificate::readFile(keypair.getPemCertificateFile()));

    // construct the PeerHello message
    ::capnp::MallocMessageBuilder message;
    generated::EnsembleMessage::Builder root = message.initRoot<generated::EnsembleMessage>();
    auto peerHello = root.initMessage().initPeerHello();
    peerHello.setCertificate(certificate->toString());

    // send the message
    auto flatArray = capnp::messageToFlatArray(message);
    auto arrayPtr = flatArray.asBytes();
    auto payload = tempo_utils::MemoryBytes::copy(
        std::span(arrayPtr.begin(), arrayPtr.end()));

    TU_RAISE_IF_NOT_OK (stream->send(MessageVersion::Version1, payload));
}

void
chord_mesh::on_supervisor_acceptor_error(const tempo_utils::Status &status, void *data)
{
    TU_LOG_WARN << "acceptor error: " << status;
}

void
chord_mesh::on_supervisor_acceptor_cleanup(void *data)
{
}

tempo_utils::Status
chord_mesh::SupervisorNode::start()
{
    StreamConnectorOps connectorOps;
    connectorOps.connect = on_supervisor_connector_connect;
    connectorOps.error = on_supervisor_connector_error;
    connectorOps.cleanup = on_supervisor_connector_cleanup;

    StreamConnectorOptions connectorOptions;
    connectorOptions.data = this;

    m_connector = std::make_unique<StreamConnector>(m_manager, connectorOps, connectorOptions);

    StreamAcceptorOps acceptorOps;
    acceptorOps.accept = on_supervisor_acceptor_accept;
    acceptorOps.error = on_supervisor_acceptor_error;
    acceptorOps.cleanup = on_supervisor_acceptor_cleanup;

    StreamAcceptorOptions acceptorOptions;
    acceptorOptions.data = this;

    TU_RETURN_IF_NOT_OK (m_acceptor->listen(acceptorOps, acceptorOptions));

    return {};
}

tempo_utils::Status
chord_mesh::SupervisorNode::connect(const chord_common::TransportLocation &endpoint)
{
    if (m_connecting.contains(endpoint))
        return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
            "connection already pending for {}", endpoint.toString());

    auto peer = std::make_unique<ConnectingPeer>();
    peer->endpoint = endpoint;
    peer->supervisor = this;
    peer->startedAt = absl::Now();
    m_connecting[endpoint] = std::move(peer);

    TU_RETURN_IF_NOT_OK (m_connector->connectLocation(endpoint, peer.get()));

    return {};
}

tempo_utils::Status
chord_mesh::SupervisorNode::shutdown()
{
    return {};
}