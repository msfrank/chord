#ifndef CHORD_HTTP_SERVER_HTTP_ACCEPTOR_H
#define CHORD_HTTP_SERVER_HTTP_ACCEPTOR_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <tempo_utils/status.h>

namespace chord_http_server {

    class HttpAcceptor : public std::enable_shared_from_this<HttpAcceptor> {
    public:
        explicit HttpAcceptor(boost::asio::io_context &ioctx);

        tempo_utils::Status initialize(boost::asio::ip::tcp::endpoint endpoint);

        void run();

    private:
        boost::asio::io_context& m_ioctx;
        boost::asio::ip::tcp::acceptor m_acceptor;

        void doAccept();
        void onAccept(boost::beast::error_code ec, boost::asio::ip::tcp::socket socket);
    };
}

#endif // CHORD_HTTP_SERVER_HTTP_ACCEPTOR_H
