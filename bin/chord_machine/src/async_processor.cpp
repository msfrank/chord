
#include <tempo_utils/log_stream.h>

#include <chord_machine/async_processor.h>

chord_machine::BaseAsyncProcessor::BaseAsyncProcessor()
    : m_async(nullptr)
{
}

chord_machine::BaseAsyncProcessor::~BaseAsyncProcessor()
{
    absl::MutexLock locker(&m_lock);
    if (m_async) {
        uv_close((uv_handle_t *) m_async, nullptr);
        delete m_async;
    }
    while (!m_queue.empty()) {
        auto *message = m_queue.front();
        m_queue.pop();
        TU_LOG_WARN << "dropping unhandled message: " << message->toString();
        delete message;
    }
}

void
chord_machine::on_message_receive(uv_async_t *async)
{
    auto *queue = static_cast<BaseAsyncProcessor *>(async->data);
    queue->processAvailableMessages();
}

tempo_utils::Status
chord_machine::BaseAsyncProcessor::initialize(uv_loop_t *loop)
{
    absl::MutexLock locker(&m_lock);
    TU_ASSERT (m_async == nullptr);
    m_async = new uv_async_t;
    uv_async_init(loop, m_async, on_message_receive);
    m_async->data = this;
    if (!m_queue.empty()) {
        uv_async_send(m_async);
    }
    return {};
}

void
chord_machine::BaseAsyncProcessor::sendAbstractMessage(AbstractMessage *message)
{
    absl::MutexLock locker(&m_lock);
    m_queue.push(message);
    // if processor is not yet initialized then do not send the signal
    if (m_async != nullptr) {
        uv_async_send(m_async);
    }
}

void
chord_machine::BaseAsyncProcessor::runUntilCancelled()
{
    {
        absl::MutexLock locker(&m_lock);
        TU_ASSERT (m_async != nullptr);
    }
    uv_run(m_async->loop, UV_RUN_DEFAULT);
}

void
chord_machine::BaseAsyncProcessor::processAvailableMessages()
{
    std::queue<AbstractMessage *> incoming;

    {
        absl::MutexLock locker(&m_lock);
        m_queue.swap(incoming);
    }

    while (!incoming.empty()) {
        auto *message = incoming.front();
        incoming.pop();
        processAbstractMessage(message);
    }
}

void
chord_machine::BaseAsyncProcessor::cancelProcessing()
{
    uv_stop(m_async->loop);
}
