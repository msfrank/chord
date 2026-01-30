
#include <chord_mesh/mesh_result.h>
#include <chord_mesh/message.h>

chord_mesh::FlatArrayBytes::FlatArrayBytes(kj::Array<capnp::word> &&flatArray)
    : m_flatArray(std::move(flatArray))
{
}

const tu_uint8 *
chord_mesh::FlatArrayBytes::getData() const
{
    auto bytes = m_flatArray.asBytes();
    return bytes.begin();
}

tu_uint32
chord_mesh::FlatArrayBytes::getSize() const
{
    auto bytes = m_flatArray.asBytes();
    return bytes.size();
}

tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>>
chord_mesh::BaseMessage::toBytes()
{
    return MeshStatus::forCondition(MeshCondition::kMeshInvariant,
        "toBytes unimplemented");
}

std::string
chord_mesh::BaseMessage::toString() const
{
    return {};
}
