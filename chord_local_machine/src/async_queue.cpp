
#include <tempo_utils/log_stream.h>

#include <chord_local_machine/async_queue.h>

BaseAsyncQueue::BaseAsyncQueue()
    : m_async(nullptr)
{
}

BaseAsyncQueue::~BaseAsyncQueue()
{
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
on_async_queue_receive(uv_async_t *async)
{
    //auto *queue = static_cast<BaseAsyncQueue *>(async->data);
    uv_stop(async->loop);
}

tempo_utils::Status
BaseAsyncQueue::initialize(uv_loop_t *loop)
{
    absl::MutexLock locker(&m_lock);
    TU_ASSERT (m_async == nullptr);
    m_async = new uv_async_t;
    uv_async_init(loop, m_async, on_async_queue_receive);
    if (!m_queue.empty()) {
        uv_async_send(m_async);
    }
    return tempo_utils::Status();
}

bool
BaseAsyncQueue::messagesPending()
{
    absl::MutexLock locker(&m_lock);
    return !m_queue.empty();
}

void
BaseAsyncQueue::sendAbstractMessage(AbstractMessage *message)
{
    absl::MutexLock locker(&m_lock);
    m_queue.push(message);
    // if queue is not yet initialized then do not send the signal
    if (m_async != nullptr) {
        uv_async_send(m_async);
    }
}

AbstractMessage *
BaseAsyncQueue::waitForAbstractMessage()
{
    {
        // grab the lock and check if there is a message already in the queue
        absl::MutexLock locker(&m_lock);
        if (!m_queue.empty()) {
            // pop the message
            auto *message = m_queue.front();
            m_queue.pop();
            // if there are additional messages pending then send signal
            if (!m_queue.empty()) {
                uv_async_send(m_async);
            }
            // return message immediately without blocking
            return message;
        }
    }

    // block on the main loop
    uv_run(m_async->loop, UV_RUN_DEFAULT);

    // grab the lock and check the queue again
    absl::MutexLock locker(&m_lock);
    if (m_queue.empty())
        return nullptr;

    // pop the message
    auto *message = m_queue.front();
    m_queue.pop();

    // if there are additional messages pending then send signal
    if (!m_queue.empty()) {
        uv_async_send(m_async);
    }

    return message;
}

AbstractMessage *
BaseAsyncQueue::takeAvailableAbstractMessage()
{
    absl::MutexLock locker(&m_lock);
    if (m_queue.empty())
        return nullptr;

    auto *message = m_queue.front();
    m_queue.pop();
    return message;
}
