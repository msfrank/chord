//
// #include <chord_mesh/flood.h>
// #include <chord_mesh/mesh_result.h>
//
// chord_mesh::FloodMesh::FloodMesh(std::unique_ptr<FloodPriv> &&priv)
//     : m_priv(std::move(priv))
// {
// }
//
// chord_mesh::FloodMesh::~FloodMesh()
// {
//     shutdown();
// }
//
// void
// chord_mesh::peer_receive(const Envelope &envelope, void *data)
// {
//     auto *peerPtr = (FloodMesh::PeerHandle *) data;
//     auto &network = peerPtr->network;
//     auto &priv = network->m_priv;
//
//     // invoke the receive callback if specified
//     const auto &callbacks = priv->callbacks;
//     if (callbacks.receive != nullptr) {
//         const auto &options = priv->options;
//         callbacks.receive(envelope, options.data);
//     }
// }
//
// bool
// chord_mesh::peer_negotiate(
//     std::string_view protocolName,
//     std::shared_ptr<tempo_security::X509Certificate> certificate,
//     void *data)
// {
//     auto *peerPtr = (FloodMesh::PeerHandle *) data;
//     auto &network = peerPtr->network;
//     auto &priv = network->m_priv;
//     const auto &options = priv->options;
//
//     peerPtr->certificate = certificate;
//     peerPtr->state = FloodMesh::PeerHandle::State::Active;
//     peerPtr->lastReceivedAt = absl::Now();
//
//     // if peer is responder then update the peers map
//     if (!peerPtr->stream->isInitiator()) {
//         auto id = std::move(peerPtr->id);
//         auto peerEntry = priv->peers.extract(id.toString());
//         auto peer = std::move(peerEntry.mapped());
//         auto commonName = certificate->getCommonName();
//         priv->peers[commonName] = std::move(peer);
//     }
//
//     // invoke the join callback if specified
//     const auto &callbacks = priv->callbacks;
//     if (callbacks.join != nullptr) {
//         auto commonName = certificate->getCommonName();
//         callbacks.join(commonName, options.data);
//     }
//
//     return true;
// }
//
// void
// chord_mesh::peer_accepted(std::shared_ptr<Stream> stream, void *data)
// {
//     auto *privPtr = (FloodMesh::FloodPriv *) data;
//
//     // initialize peer handle
//     auto peer = std::make_unique<FloodMesh::PeerHandle>();
//     peer->state = FloodMesh::PeerHandle::State::Negotiating;
//     peer->network = privPtr->network.lock();
//     peer->stream = stream;
//
//     auto now = absl::Now();
//     peer->addedAt = now;
//     peer->connectedAt = now;
//     peer->connectAttempts = 0;
//
//     // insert peer in the peers map
//     auto *peerPtr = peer.get();
//     auto id = tempo_utils::UUID::randomUUID();
//     peer->id = id;
//     privPtr->peers[id.toString()] = std::move(peer);
//
//     tempo_utils::Status status;
//
//     // start the stream
//     StreamOps streamOps;
//     streamOps.negotiate = peer_negotiate;
//     streamOps.receive = peer_receive;
//     status = stream->start(streamOps, peerPtr);
//     TU_LOG_ERROR_IF (status.notOk()) << "failed to start stream: " << status;
//
//     // // perform stream negotiation
//     // const auto &options = privPtr->options;
//     // status = stream->negotiate(options.protocolName);
//     // TU_LOG_ERROR_IF (status.notOk()) << "failed to negotiate stream: " << status;
// }
//
// void
// chord_mesh::accept_error(const tempo_utils::Status &status, void *data)
// {
//     TU_LOG_ERROR << "failed to accept connection: " << status;
// }
//
// void
// chord_mesh::peer_connected(std::shared_ptr<Stream> stream, void *data)
// {
//     auto *peerPtr = (FloodMesh::PeerHandle *) data;
//     auto network = peerPtr->network;
//     auto &priv = network->m_priv;
//     const auto &options = priv->options;
//
//     // update peer handle
//     peerPtr->stream = stream;
//     peerPtr->connectedAt = absl::Now();
//     peerPtr->connectAttempts = 0;
//     peerPtr->state = FloodMesh::PeerHandle::State::Negotiating;
//
//     tempo_utils::Status status;
//
//     // start the stream
//     StreamOps streamOps;
//     streamOps.negotiate = peer_negotiate;
//     streamOps.receive = peer_receive;
//     status = stream->start(streamOps, peerPtr);
//     TU_LOG_ERROR_IF (status.notOk()) << "failed to start stream: " << status;
//
//     // perform stream negotiation
//     status = stream->negotiate(options.protocolName);
//     TU_LOG_ERROR_IF (status.notOk()) << "failed to negotiate stream: " << status;
// }
//
// void
// chord_mesh::retry_connect(uv_timer_t *handle)
// {
//     auto *peerPtr = (FloodMesh::PeerHandle *) handle->data;
//     auto network = peerPtr->network;
//     auto &priv = network->m_priv;
//
//     auto connectResult = priv->connector->connectLocation(peerPtr->endpoint, peerPtr);
//     if (connectResult.isStatus()) {
//         auto status = connectResult.getStatus();
//         TU_LOG_ERROR << "connect failed: " << status;
//     }
//     peerPtr->id = connectResult.getResult();
//     peerPtr->connectAttempts++;
//     peerPtr->state = FloodMesh::PeerHandle::State::Connecting;
// }
//
// void
// chord_mesh::connect_error(const tempo_utils::Status &status, void *data)
// {
//     auto *peerPtr = (FloodMesh::PeerHandle *) data;
//     auto network = peerPtr->network;
//     auto &priv = network->m_priv;
//
//     TU_LOG_ERROR << "failed to connect to " << peerPtr->endpoint.toString() << ": " << status;
//
//     peerPtr->state = FloodMesh::PeerHandle::State::Waiting;
//     peerPtr->connectAttempts++;
//
//     auto *loop = priv->manager->getLoop();
//     uv_timer_init(loop, &peerPtr->timer);
//     peerPtr->timer.data = data;
//
//     tu_int64 delay = absl::Uniform(priv->rand, 5000, 10000);
//     uv_timer_start(&peerPtr->timer, retry_connect, delay, 0);
// }
//
// tempo_utils::Result<std::shared_ptr<chord_mesh::FloodMesh>>
// chord_mesh::FloodMesh::create(
//     const chord_common::TransportLocation &listenEndpoint,
//     StreamManager *manager,
//     const FloodCallbacks &callbacks,
//     const FloodOptions &options)
// {
//     TU_ASSERT (manager != nullptr);
//
//     auto priv = std::make_unique<FloodPriv>();
//     priv->callbacks = callbacks;
//     priv->options = options;
//
//     TU_ASSIGN_OR_RETURN (priv->acceptor, StreamAcceptor::forLocation(listenEndpoint, manager));
//
//     StreamConnectorOps connectorOps;
//     connectorOps.connect = peer_connected;
//     connectorOps.error = connect_error;
//
//     StreamConnectorOptions connectorOptions;
//     connectorOptions.data = priv.get();
//     connectorOptions.startInsecure = options.allowInsecure;
//
//     TU_ASSIGN_OR_RETURN (priv->connector, StreamConnector::create(manager, connectorOps, connectorOptions));
//
//     StreamAcceptorOps acceptorOps;
//     acceptorOps.accept = peer_accepted;
//     acceptorOps.error = accept_error;
//
//     StreamAcceptorOptions acceptorOptions;
//     acceptorOptions.data = priv.get();
//     acceptorOptions.allowInsecure = options.allowInsecure;
//
//     TU_RETURN_IF_NOT_OK (priv->acceptor->listen(acceptorOps, acceptorOptions));
//
//     auto flood = std::shared_ptr<FloodMesh>(new FloodMesh(std::move(priv)));
//     flood->m_priv->network = flood->weak_from_this();
//
//     for (const auto &peerEndpoint : options.initialPeerEndpoints) {
//         TU_RETURN_IF_NOT_OK (flood->addPeer(peerEndpoint));
//     }
//
//     return flood;
// }
//
// tempo_utils::Status
// chord_mesh::FloodMesh::addPeer(const chord_common::TransportLocation &peerEndpoint)
// {
//     if (m_priv->shutdown)
//         return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
//             "flood mesh is shut down");
//
//     auto peerId = peerEndpoint.getServerName();
//     if (m_priv->peers.contains(peerId))
//         return {};
//
//     auto peer = std::make_unique<PeerHandle>();
//     peer->state = PeerHandle::State::Connecting;
//     peer->endpoint = peerEndpoint;
//     peer->network = shared_from_this();
//     peer->addedAt = absl::Now();
//     peer->connectAttempts = 1;
//
//     TU_ASSIGN_OR_RETURN (peer->id, m_priv->connector->connectLocation(peerEndpoint, peer.get()));
//
//     m_priv->peers[peerId] = std::move(peer);
//
//     return {};
// }
//
// tempo_utils::Status
// chord_mesh::FloodMesh::removePeer(std::string_view peerId)
// {
//     auto peerEntry = m_priv->peers.extract(peerId);
//     if (peerEntry.empty())
//         return {};
//     auto peer = std::move(peerEntry.mapped());
//
//     switch (peer->state) {
//         case PeerHandle::State::Closing:
//             return {};
//
//         case PeerHandle::State::Connecting:
//             TU_RETURN_IF_NOT_OK (m_priv->connector->abort(peer->id));
//             peer->state = PeerHandle::State::Closing;
//             break;
//
//         case PeerHandle::State::Negotiating:
//         case PeerHandle::State::Active:
//             peer->stream->shutdown();
//             peer->state = PeerHandle::State::Closing;
//             break;
//
//         case PeerHandle::State::Waiting:
//             uv_timer_stop(&peer->timer);
//             break;
//
//         default:
//             return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
//                 "invalid peer state");
//     }
//
//     return {};
// }
//
// tempo_utils::Status
// chord_mesh::FloodMesh::sendMessage(
//     std::string_view peerId,
//     std::shared_ptr<const tempo_utils::ImmutableBytes> payload)
// {
//     if (m_priv->shutdown)
//         return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
//             "flood mesh is shut down");
//
//     auto entry = m_priv->peers.find(peerId);
//     if (entry == m_priv->peers.cend())
//         return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
//             "unknown peer '{}'", peerId);
//
//     auto &peer = entry->second;
//     if (peer->state != PeerHandle::State::Active)
//         return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
//             "peer '{}' is not active", peerId);
//
//     TU_RETURN_IF_NOT_OK (peer->stream->send(EnvelopeVersion::Version1, payload));
//     return {};
// }
//
// tempo_utils::Status
// chord_mesh::FloodMesh::broadcastMessage(std::shared_ptr<const tempo_utils::ImmutableBytes> payload)
// {
//     if (m_priv->shutdown)
//         return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
//             "flood mesh is shut down");
//
//     auto timestamp = absl::Now();
//
//     for (auto &entry : m_priv->peers) {
//         auto &peer = entry.second;
//         if (peer->state != PeerHandle::State::Active)
//             continue;
//         TU_RETURN_IF_NOT_OK (peer->stream->send(EnvelopeVersion::Version1, payload, timestamp));
//     }
//
//     return {};
// }
//
// void
// chord_mesh::FloodMesh::shutdown()
// {
//     if (m_priv->shutdown)
//         return;
//     m_priv->shutdown = true;
//
//     m_priv->acceptor->shutdown();
//
//     while (!m_priv->peers.empty()) {
//         auto entry = m_priv->peers.begin();
//         removePeer(entry->first);
//     }
//
//     m_priv->connector->shutdown();
// }
