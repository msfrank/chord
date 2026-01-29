@0x8f4d20efe2bd01e1;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("test_generated");

struct Request {
    value @0 :Text;
}

struct Reply {
    value @0 :Text;
}