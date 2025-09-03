
#include <chord_http_server/http_service.h>
#include <tempo_utils/log_stream.h>

chord_http_server::HttpService::HttpService(int numWorkers)
    : m_ioctx(numWorkers + 1),
      m_workerThreads(numWorkers)
{
    m_acceptor = std::make_shared<HttpAcceptor>(m_ioctx);
}

tempo_utils::Status
chord_http_server::HttpService::initialize(boost::asio::ip::tcp::endpoint endpoint)
{
    return m_acceptor->initialize(endpoint);
}

static void
listener_thread(void *data)
{
    auto *acceptor = static_cast<chord_http_server::HttpAcceptor *>(data);
    TU_ASSERT (acceptor != nullptr);
    TU_LOG_INFO << "started listener thread";
    acceptor->run();
    TU_LOG_INFO << "stopped listener thread";
}

static void
worker_thread(void *data)
{
    auto *ioctxptr = static_cast<boost::asio::io_context *>(data);
    TU_ASSERT (ioctxptr != nullptr);
    TU_LOG_INFO << "started worker thread";
    ioctxptr->run();
    TU_LOG_INFO << "stopped worker thread";
}

tempo_utils::Status
chord_http_server::HttpService::run()
{
    TU_LOG_INFO << "starting http service";
    // start worker threads first
    for (int i = 0; i < m_workerThreads.size(); i++) {
        uv_thread_create(&m_workerThreads[i], worker_thread, &m_ioctx);
    }
    // then start listener thread to start accepting connections
    uv_thread_create(&m_listenerThread, listener_thread, m_acceptor.get());
    return {};
}

tempo_utils::Status
chord_http_server::HttpService::shutdown()
{
    TU_LOG_INFO << "stopping http service";
    // signal io context to stop processing
    m_ioctx.stop();
    // this will cause threads to terminate, so we can join them
    uv_thread_join(&m_listenerThread);
    for (int i = 0; i < m_workerThreads.size(); i++) {
        uv_thread_join(&m_workerThreads[i]);
    }
    return {};
}
