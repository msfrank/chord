#ifndef CHORD_MESH_REP_PROTOCOL_H
#define CHORD_MESH_REP_PROTOCOL_H

#include "req_protocol.h"
#include "stream_manager.h"
#include "stream.h"

namespace chord_mesh {

    template<class ReqMessage, class RspMessage>
    class AbstractRepProtocol {
    public:
        virtual ~AbstractRepProtocol() = default;
        virtual tempo_utils::Status reply(const ReqMessage &request, RspMessage &reply) = 0;
        virtual tempo_utils::Status validate(
            std::string_view protocolName,
            std::shared_ptr<tempo_security::X509Certificate> certificate) = 0;
        virtual void error(const tempo_utils::Status &status) = 0;
        virtual void cleanup() = 0;
    };

    class RepStreamImpl : public AbstractStreamContext {
    public:
        tempo_utils::Status validate(
            std::string_view protocolName,
            std::shared_ptr<tempo_security::X509Certificate> certificate) override;
        void error(const tempo_utils::Status &) override;
        void cleanup() override;
    };

    template<class ReqMessage, class RspMessage>
    class RepStream : public RepStreamImpl {
    public:
        RepStream(
            std::unique_ptr<AbstractRepProtocol<ReqMessage,RspMessage>> &&protocol,
            std::shared_ptr<Stream> stream)
            : m_protocol(std::move(protocol)),
              m_stream(std::move(stream))
        {
            TU_ASSERT (m_protocol != nullptr);
            TU_ASSERT (m_stream != nullptr);
        }

        void receive(const Envelope &envelope) override
        {
            auto payload = envelope.getPayload();
            ReqMessage request;
            auto status = request.parse(payload);
            if (status.notOk()) {
                error(status);
                return;
            }
            RspMessage reply;
            status = m_protocol->reply(request, reply);
            if (status.notOk()) {
                error(status);
                return;
            }
            auto result = reply.toBytes();
            if (result.isStatus()) {
                error(status);
                return;
            }
            auto bytes = result.getResult();
            status = m_stream->send(envelope.getVersion(), bytes);
            if (status.notOk()) {
                error(status);
            }
        }


    private:
        std::unique_ptr<AbstractRepProtocol<ReqMessage,RspMessage>> m_protocol;
        std::shared_ptr<Stream> m_stream;
    };

    template<class ReqMessage, class RspMessage>
    class RepAcceptor : public AbstractAcceptContext {
    public:
        /**
         *
         * @return
         */
        virtual std::unique_ptr<AbstractRepProtocol<ReqMessage,RspMessage>> make() = 0;

        void accept(std::shared_ptr<Stream> stream) override
        {
            auto ctx = make();
            auto protocol = std::make_unique<ReqProtocol<ReqMessage,RspMessage>>(std::move(ctx), stream);
            auto status = stream->start(std::move(protocol));
            if (status.notOk()) {
                error(status);
            }
        }
    };
}

#endif // CHORD_MESH_REP_PROTOCOL_H