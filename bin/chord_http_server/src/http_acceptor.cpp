
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>

#include <chord_http_server/http_acceptor.h>
#include <chord_http_server/http_session.h>
#include <tempo_utils/log_stream.h>

chord_http_server::HttpAcceptor::HttpAcceptor(boost::asio::io_context &ioctx)
    : std::enable_shared_from_this<HttpAcceptor>(),
      m_ioctx(ioctx),
      m_acceptor(boost::asio::make_strand(ioctx))
{
}

tempo_utils::Status
chord_http_server::HttpAcceptor::initialize(boost::asio::ip::tcp::endpoint endpoint)
{
    boost::beast::error_code ec;

    // open the acceptor
    m_acceptor.open(endpoint.protocol(), ec);
    if(ec)
        return tempo_utils::GenericStatus::forCondition(tempo_utils::GenericCondition::kInternalViolation,
            "failed to open listening socket: {}", ec.message());

    // enable address reuse
    m_acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
    if(ec)
        return tempo_utils::GenericStatus::forCondition(tempo_utils::GenericCondition::kInternalViolation,
            "failed to set socket option: {}", ec.message());

    // bind to the server address
    m_acceptor.bind(endpoint, ec);
    if(ec)
        return tempo_utils::GenericStatus::forCondition(tempo_utils::GenericCondition::kInternalViolation,
            "failed to bind socket: {}", ec.message());

    // start listening for connections
    m_acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if(ec)
        return tempo_utils::GenericStatus::forCondition(tempo_utils::GenericCondition::kInternalViolation,
            "failed to listen on socket: {}", ec.message());

    TU_LOG_INFO << "listening on endpoint";
    doAccept();

    return {};
}


void
chord_http_server::HttpAcceptor::run()
{
    m_ioctx.run();
}

void
chord_http_server::HttpAcceptor::doAccept()
{
    m_acceptor.async_accept(
        boost::asio::make_strand(m_ioctx),
        boost::beast::bind_front_handler(
            &HttpAcceptor::onAccept,
            shared_from_this()));
    TU_LOG_INFO << "async_accept";
}

void
chord_http_server::HttpAcceptor::onAccept(
    boost::beast::error_code ec,
    boost::asio::ip::tcp::socket socket)
{
    if(ec) {
        TU_LOG_ERROR << "failed to listen on socket: " << ec.message();
        return; // To avoid infinite loop
    }

    TU_LOG_INFO << "accepted connection";

    // Create the session and run it
    auto session = std::make_shared<HttpSession>(std::move(socket));
    session->run();

    // Accept another connection
    doAccept();
}