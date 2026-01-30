#ifndef CHORD_MESH_MESSAGE_H
#define CHORD_MESH_MESSAGE_H

#include <capnp/serialize.h>

#include <tempo_utils/immutable_bytes.h>
#include <tempo_utils/memory_bytes.h>
#include <tempo_utils/result.h>

namespace chord_mesh {

    class BaseMessage {
    public:
        virtual ~BaseMessage() = default;

        virtual tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>> toBytes();

        virtual std::string toString() const;
    };

    class FlatArrayBytes : public tempo_utils::ImmutableBytes {
    public:
        explicit FlatArrayBytes(kj::Array<capnp::word> &&flatArray);
        const tu_uint8* getData() const override;
        tu_uint32 getSize() const override;

    private:
        kj::Array<capnp::word> m_flatArray;
    };

    /**
     *
     * @tparam T
     */
    template <typename T>
    class Message : public BaseMessage {
    public:
        Message(): m_inner(std::make_unique<capnp::MallocMessageBuilder>()) {};

        Message(Message &&other) noexcept
        {
            m_inner = std::move(other.m_inner);
        };


        Message& operator=(Message &&other) noexcept
        {
            if (this != &other) {
                m_inner = std::move(other.m_inner);
            }
            return *this;
        }

        Message(const Message &other) = delete;
        Message& operator=(const Message &other) = delete;

        T::Builder getRoot()
        {
            return m_inner->getRoot<T>();
        }

        T::Reader getRoot() const
        {
            return m_inner->getRoot<T>();
        }

        tempo_utils::Status parse(std::shared_ptr<const tempo_utils::ImmutableBytes> payload)
        {
            auto arrayPtr = kj::arrayPtr(payload->getData(), payload->getSize());
            kj::ArrayInputStream inputStream(arrayPtr);
            capnp::readMessageCopy(inputStream, *m_inner);
            return {};
        }

        tempo_utils::Result<std::shared_ptr<const tempo_utils::ImmutableBytes>> toBytes() override
        {
            auto flatArray = capnp::messageToFlatArray(*m_inner);
            auto bytes = std::make_shared<FlatArrayBytes>(std::move(flatArray));
            return std::static_pointer_cast<const tempo_utils::ImmutableBytes>(bytes);
        }

        std::string toString() const override
        {
            auto root = getRoot();
            auto s = root.toString().flatten();
            return {s.begin(), s.end()};
        }

    private:
        std::unique_ptr<capnp::MallocMessageBuilder> m_inner;
    };
}

#endif // CHORD_MESH_MESSAGE_H