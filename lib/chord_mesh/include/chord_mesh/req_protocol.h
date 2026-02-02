#ifndef CHORD_MESH_REQ_PROTOCOL_H
#define CHORD_MESH_REQ_PROTOCOL_H

#include <chord_common/transport_location.h>

#include "stream.h"
#include "stream_connector.h"

namespace chord_mesh {

    struct ReqProtocolOptions {
        bool startInsecure = false;
        void *data = nullptr;
    };

    /**
     *
     */
    class ReqProtocolImpl : public std::enable_shared_from_this<ReqProtocolImpl> {
    public:
        ReqProtocolImpl(std::shared_ptr<StreamConnector> connector, const ReqProtocolOptions &options);
        virtual ~ReqProtocolImpl() = default;

        tempo_utils::Status connect(const chord_common::TransportLocation &location);
        void shutdown();

    protected:
        ReqProtocolOptions m_options;

        tempo_utils::Result<tu_uint32> send(std::shared_ptr<const tempo_utils::ImmutableBytes> payload);

        virtual tempo_utils::Status receive(
            tu_uint32 id,
            std::shared_ptr<const tempo_utils::ImmutableBytes> payload) = 0;

        virtual void emitError(const tempo_utils::Status &status);

    private:
        std::shared_ptr<StreamConnector> m_connector;
        std::shared_ptr<Connect> m_connect;
        std::shared_ptr<Stream> m_stream;
        tu_uint32 m_currId = 0;
        std::queue<std::pair<tu_uint32,std::shared_ptr<const tempo_utils::ImmutableBytes>>> m_pending;

        class ReqConnectContext : public AbstractConnectContext {
        public:
            ReqConnectContext(std::weak_ptr<ReqProtocolImpl> impl);

            void connect(std::shared_ptr<Stream> stream) override;
            void error(const tempo_utils::Status &status) override;
            void cleanup() override;

        private:
            std::weak_ptr<ReqProtocolImpl> m_impl;
        };

        class ReqStreamContext : public AbstractStreamContext {
        public:
            ReqStreamContext(std::weak_ptr<ReqProtocolImpl> impl);

            tempo_utils::Status validate(
                std::string_view protocolName,
                std::shared_ptr<tempo_security::X509Certificate> certificate) override;
            void receive(const Envelope &envelope) override;
            void error(const tempo_utils::Status &status) override;
            void cleanup() override;

        private:
            std::weak_ptr<ReqProtocolImpl> m_impl;
        };

        friend class ReqConnectContext;
        friend class ReqStreamContext;
    };

    /**
     *
     * @tparam ReqMessage
     * @tparam RspMessage
     */
    template<class ReqMessage, class RspMessage>
    class ReqProtocol : public ReqProtocolImpl {
    public:

        class AbstractContext {
        public:
            virtual ~AbstractContext() = default;
            virtual void receive(const RspMessage &message) = 0;
            virtual void error(const tempo_utils::Status &status) = 0;
            virtual void cleanup() = 0;
        };

    private:
        struct Private{ explicit Private() = default; };

    public:
        ReqProtocol(
            std::shared_ptr<StreamConnector> connector,
            std::unique_ptr<AbstractContext> &&ctx,
            const ReqProtocolOptions &options,
            Private)
                : ReqProtocolImpl(connector, options),
                  m_ctx(std::move(ctx))
        {
            TU_ASSERT (m_ctx != nullptr);
        }

        ~ReqProtocol() override { m_ctx->cleanup(); }

        /**
         *
         * @param connector
         * @param ctx
         * @param options
         * @return
         */
        static tempo_utils::Result<std::shared_ptr<ReqProtocol>> create(
            std::shared_ptr<StreamConnector> connector,
            std::unique_ptr<AbstractContext> &&ctx,
            const ReqProtocolOptions &options = {})
        {
            return std::make_shared<ReqProtocol>(std::move(connector), std::move(ctx), options, Private{});
        }

        /**
         *
         * @param req
         * @return
         */
        tempo_utils::Result<tu_uint32> send(ReqMessage &&req)
        {
            std::shared_ptr<const tempo_utils::ImmutableBytes> bytes;
            TU_ASSIGN_OR_RETURN (bytes, req.toBytes());
            return ReqProtocolImpl::send(bytes);
        }

    protected:
        tempo_utils::Status receive(
            tu_uint32 id,
            std::shared_ptr<const tempo_utils::ImmutableBytes> payload) override
        {
            RspMessage message;
            TU_RETURN_IF_NOT_OK (message.parse(payload));
            m_ctx->receive(message);
            return {};
        }

    private:
        std::unique_ptr<AbstractContext> m_ctx;
    };
}

#endif // CHORD_MESH_REQ_PROTOCOL_H