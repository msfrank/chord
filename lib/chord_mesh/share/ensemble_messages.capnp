@0x886200adb5d12de0;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("chord_mesh::generated");

struct EnsembleMessage {

    enum NodeType {
        user @0;
        idp @1;
        supervisor @2;
        machine @3;
    }

    struct PeerHello {
        certificate @0 :Text;
    }

    struct NodeJoined {
        type @0 :NodeType;
        endpoint @1 :Text;
    }

    struct NodeLeft {
        endpoint @0 :Text;
    }

    struct PortOpened {
        protocol @0 :Text;
        endpoint @1 :Text;
    }

    struct PortClosed {
        endpoint @0 :Text;
    }

    struct CertificateSigned {
        certificate @0 :Text;
        endpoint @1 :Text;
    }

    struct GossipGraft {
    }

    struct GossipPrune {
    }

    struct GossipIHave {
    }

    struct GossipIWant {
    }

    message :union {
        peerHello @0 :PeerHello;
        nodeJoined @1 :NodeJoined;
        nodeLeft @2 :NodeLeft;
        portOpened @3 :PortOpened;
        portClosed @4 :PortClosed;
        certSigned @5 :CertificateSigned;
        gossipGraft @6 :GossipGraft;
        gossipPrune @7 :GossipPrune;
        gossipIHave @8 :GossipIHave;
        gossipIWant @9 :GossipIWant;
    }
}