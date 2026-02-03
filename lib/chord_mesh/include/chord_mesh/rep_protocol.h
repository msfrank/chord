#ifndef CHORD_MESH_REP_PROTOCOL_H
#define CHORD_MESH_REP_PROTOCOL_H

#include "req_protocol.h"
#include "stream_manager.h"
#include "stream.h"

namespace chord_mesh {

    template<class ReqMessage, class RepMessage>
    class AbstractRepProtocol {
    public:
        virtual ~AbstractRepProtocol() = default;
        virtual tempo_utils::Status reply(
            AbstractCloseable *closeable,
            const ReqMessage &request,
            RepMessage &reply) = 0;
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

    template<class ReqMessage, class RepMessage>
    class RepStream : public RepStreamImpl, public AbstractCloseable {
    public:
        RepStream(
            std::unique_ptr<AbstractRepProtocol<ReqMessage,RepMessage>> &&protocol,
            std::shared_ptr<Stream> stream)
            : m_protocol(std::move(protocol)),
              m_stream(std::move(stream)),
              m_shutdown(false),
              m_close(false)
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
            RepMessage reply;
            status = m_protocol->reply(this, request, reply);
            if (status.notOk()) {
                error(status);
                return;
            }
            if (m_close) {
                m_stream->close();
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
            if (m_shutdown) {
                m_stream->shutdown();
            }
        }
        void shutdown() override {
            m_shutdown = true;
        }
        void close() override {
            m_close = true;
        }

    private:
        std::unique_ptr<AbstractRepProtocol<ReqMessage,RepMessage>> m_protocol;
        std::shared_ptr<Stream> m_stream;
        bool m_shutdown;
        bool m_close;
    };

    template<class ReqMessage, class RepMessage>
    class RepAcceptContext : public AbstractAcceptContext {
    public:
        /**
         *
         * @return
         */
        virtual std::unique_ptr<AbstractRepProtocol<ReqMessage,RepMessage>> make() = 0;

        void accept(std::shared_ptr<Stream> stream) override
        {
            auto ctx = make();
            auto rep = std::make_unique<RepStream<ReqMessage,RepMessage>>(std::move(ctx), stream);
            auto status = stream->start(std::move(rep));
            if (status.notOk()) {
                error(status);
            }
        }
    };
}

#endif // CHORD_MESH_REP_PROTOCOL_H