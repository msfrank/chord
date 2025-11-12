@0xb0af753839871d9e;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("chord_mesh::generated");

struct StreamMessage {

    struct StreamNegotiate {
        publicKey @0 :Data;
        certificate @1 :Text;
        digest @2 :Data;
        protocol @3 :Text;
    }

    struct StreamHandshake {
        data @0 :Data;
    }

    struct StreamError {
        code @0 :UInt8;
        message @1 :Text;
    }

    message :union {
        streamNegotiate @0 :StreamNegotiate;
        streamHandshake @1 :StreamHandshake;
        streamError @2 :StreamError;
    }
}
