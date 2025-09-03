#ifndef CHORD_HTTP_SERVER_HTTP_SERVICE_H
#define CHORD_HTTP_SERVER_HTTP_SERVICE_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <uv.h>

#include <tempo_utils/status.h>

#include "http_acceptor.h"

namespace chord_http_server {

    class HttpService {
    public:
        explicit HttpService(int numWorkers);

        tempo_utils::Status initialize(boost::asio::ip::tcp::endpoint endpoint);
        tempo_utils::Status run();
        tempo_utils::Status shutdown();

    private:
        boost::asio::io_context m_ioctx;
        std::shared_ptr<HttpAcceptor> m_acceptor;
        std::vector<uv_thread_t> m_workerThreads;
        uv_thread_t m_listenerThread;
    };
}

#endif // CHORD_HTTP_SERVER_HTTP_SERVICE_H
