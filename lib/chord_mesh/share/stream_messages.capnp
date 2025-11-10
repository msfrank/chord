@0xb0af753839871d9e;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("chord_mesh::generated");

struct StreamMessage {

    struct StreamHello {
        publicKey @0 :Data;
        certificate @1 :Text;
        digest @2 :Data;
    }

    struct StreamError {
        code @0 :UInt8;
        message @1 :Text;
    }

    message :union {
        streamHello @0 :StreamHello;
        streamError @1 :StreamError;
    }
}
