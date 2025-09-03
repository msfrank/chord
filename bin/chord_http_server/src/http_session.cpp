
#include <chord_http_server/http_session.h>
#include <tempo_utils/log_stream.h>

chord_http_server::HttpSession::HttpSession(boost::asio::ip::tcp::socket&& socket)
    : std::enable_shared_from_this<HttpSession>(),
      m_stream(std::move(socket))
{
}

void
chord_http_server::HttpSession::run()
{
    TU_LOG_INFO << "starting http session";
    boost::asio::dispatch(m_stream.get_executor(),
                  boost::beast::bind_front_handler(
                      &HttpSession::doRead, shared_from_this()));
}

void
chord_http_server::HttpSession::doRead()
{
    // otherwise the operation behavior is undefined.
    m_req = {};

    // Set the timeout.
    m_stream.expires_after(std::chrono::seconds(30));

    // Read a request
    boost::beast::http::async_read(
        m_stream, m_buffer, m_req, boost::beast::bind_front_handler(
            &HttpSession::handleRequest, shared_from_this()));
}

void
chord_http_server::HttpSession::handleRequest(
    boost::beast::error_code ec,
    std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    // This means they closed the connection
    if(ec == boost::beast::http::error::end_of_stream)
        return doClose();

    if(ec) {
        TU_LOG_ERROR << "request failed: " << ec.message();
        return;
    }

    TU_LOG_INFO << "request: " << std::string(m_req.method_string())
        << " " << std::string(m_req.target())
        << " " << m_req.version();
    for (const auto &field : m_req) {
        TU_LOG_INFO << "header: " << std::string(field.name_string()) << ": " << std::string(field.value());
    }

    std::string body{"this is the response body.\n"};
    boost::beast::http::response<boost::beast::http::string_body> rsp;
    rsp.result(boost::beast::http::status::ok);
    rsp.reason("S'all good!");
    rsp.body() = body;
    rsp.content_length(body.size());
    rsp.keep_alive(false);
    rsp.prepare_payload();

    // Send the response
    sendResponse(std::move(rsp));
}

void
chord_http_server::HttpSession::sendResponse(boost::beast::http::message_generator&& msg)
{
    bool keep_alive = msg.keep_alive();

    // Write the response
    boost::beast::async_write(
        m_stream,
        std::move(msg),
        boost::beast::bind_front_handler(
            &HttpSession::onWrite, shared_from_this(), keep_alive));
}

void
chord_http_server::HttpSession::onWrite(
    bool keep_alive,
    boost::beast::error_code ec,
    std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if(ec) {
        TU_LOG_ERROR << "write failed: " << ec.message();
        return;
    }

    if(! keep_alive)
    {
        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        return doClose();
    }

    // Read another request
    doRead();
}

void
chord_http_server::HttpSession::doClose()
{
    // Send a TCP shutdown
    boost::beast::error_code ec;
    m_stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
}

