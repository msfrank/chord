#ifndef CHORD_MESH_REQ_PROTOCOL_H
#define CHORD_MESH_REQ_PROTOCOL_H

#include <capnp/message.h>
#include <capnp/serialize.h>

#include <chord_common/transport_location.h>

#include "stream.h"
#include "stream_connector.h"

namespace chord_mesh {

    typedef void (*ReqProtocolErrorCallback)(const tempo_utils::Status &, void *);
    typedef void (*ReqProtocolCleanupCallback)(void *);

    struct ReqProtocolOptions {
        bool startInsecure = false;
        void *data = nullptr;
    };

    /**
     *
     */
    class ReqProtocolImpl {
    public:
        ReqProtocolImpl(
            StreamManager *manager,
            ReqProtocolErrorCallback error,
            ReqProtocolCleanupCallback cleanup);
        virtual ~ReqProtocolImpl() = default;

        tempo_utils::Status connect(const chord_common::TransportLocation &location);
        void shutdown();

    protected:
        tempo_utils::Result<tu_uint32> send(::capnp::MessageBuilder &builder);

        virtual tempo_utils::Status receive(
            tu_uint32 id,
            std::shared_ptr<const tempo_utils::ImmutableBytes> payload) = 0;

    private:
        StreamManager *m_manager;
        ReqProtocolErrorCallback m_error;
        ReqProtocolCleanupCallback m_cleanup;
        std::shared_ptr<StreamConnector> m_connector;
        tempo_utils::UUID m_connectId;
        std::shared_ptr<Stream> m_stream;
        tu_uint32 m_currId = 0;
        std::queue<std::pair<tu_uint32,std::shared_ptr<tempo_utils::ImmutableBytes>>> m_pending;

        friend void on_connect_complete(std::shared_ptr<Stream> stream, void *data);
        friend void on_connect_error(const tempo_utils::Status &status, void *data);
        friend void on_stream_receive(const Message &message, void *data);
    };

    /**
     *
     * @tparam ReqMessage
     * @tparam RspMessage
     */
    template<class ReqMessage, class RspMessage>
    class ReqProtocol : public ReqProtocolImpl {
    public:

        struct Callbacks {
            void (*receive)(const typename RspMessage::Reader &, void *) = nullptr;
            void (*error)(const tempo_utils::Status &, void *) = nullptr;
            void (*cleanup)(void *) = nullptr;
        };

    private:
        struct Private{ explicit Private() = default; };

    public:
        ReqProtocol(
            StreamManager *manager,
            const Callbacks &callbacks,
            const ReqProtocolOptions &options,
            Private)
                : ReqProtocolImpl(manager, callbacks.error, callbacks.cleanup),
                  m_callbacks(callbacks),
                  m_options(options)
        {
        }

        /**
         *
         * @param endpoint
         * @param manager
         * @param callbacks
         * @param options
         * @return
         */
        static tempo_utils::Result<std::shared_ptr<ReqProtocol>> create(
            StreamManager *manager,
            const Callbacks &callbacks,
            const ReqProtocolOptions &options = {})
        {
            return std::make_shared<ReqProtocol>(manager, callbacks, options, Private{});
        }

        /**
         *
         * @param req
         * @return
         */
        tempo_utils::Result<tu_uint32> send(ReqMessage::Reader &&req)
        {
            ::capnp::MallocMessageBuilder builder;
            builder.setRoot(std::move(req));
            return send(builder);
        }

    protected:
        tempo_utils::Status receive(
            tu_uint32 id,
            std::shared_ptr<const tempo_utils::ImmutableBytes> payload) override
        {
            auto arrayPtr = kj::arrayPtr(payload->getData(), payload->getSize());
            kj::ArrayInputStream inputStream(arrayPtr);
            capnp::MallocMessageBuilder builder;
            capnp::readMessageCopy(inputStream, builder);
            auto root = builder.getRoot<RspMessage>();
            m_callbacks.receive(root.asReader(), m_options.data);
            return {};
        }

    private:
        Callbacks m_callbacks;
        ReqProtocolOptions m_options;
    };
}

#endif // CHORD_MESH_REQ_PROTOCOL_H