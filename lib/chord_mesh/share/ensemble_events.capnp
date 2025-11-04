@0x886200adb5d12de0;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("chord_mesh::generated");

struct EnsembleEvent {

    enum NodeType {
        user @0;
        idp @1;
        supervisor @2;
        machine @3;
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
        certificate @0 :Data;
        endpoint @1 :Text;
    }

    event :union {
        nodeJoined @0 :NodeJoined;
        nodeLeft @1 :NodeLeft;
        portOpened @2 :PortOpened;
        portClosed @3 :PortClosed;
        certificateSigned @4 :CertificateSigned;
    }
}