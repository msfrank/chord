#ifndef CHORD_HTTP_SERVER_HTTP_SESSION_H
#define CHORD_HTTP_SERVER_HTTP_SESSION_H

#include <boost/beast.hpp>

namespace chord_http_server {

    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    public:
        explicit HttpSession(boost::asio::ip::tcp::socket&& socket);

        void run();

    private:
        boost::beast::tcp_stream m_stream;
        boost::beast::flat_buffer m_buffer;
        boost::beast::http::request<boost::beast::http::string_body> m_req;

        void doRead();
        void handleRequest(boost::beast::error_code ec, std::size_t bytes_transferred);
        void sendResponse(boost::beast::http::message_generator&& msg);
        void onWrite(bool keep_alive, boost::beast::error_code ec, std::size_t bytes_transferred);
        void doClose();
    };
}

#endif // CHORD_HTTP_SERVER_HTTP_SESSION_H
