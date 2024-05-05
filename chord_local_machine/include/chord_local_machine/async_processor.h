#ifndef CHORD_LOCAL_MACHINE_ASYNC_PROCESSOR_H
#define CHORD_LOCAL_MACHINE_ASYNC_PROCESSOR_H

#include <queue>

#include <absl/synchronization/mutex.h>
#include <uv.h>

#include <tempo_utils/status.h>

#include "abstract_message_sender.h"

/**
 *
 */
class BaseAsyncProcessor {
public:
    BaseAsyncProcessor();
    virtual ~BaseAsyncProcessor();

    tempo_utils::Status initialize(uv_loop_t *loop);
    void processAvailableMessages();
    void runUntilCancelled();

protected:
    virtual void processAbstractMessage(AbstractMessage *message) = 0;
    void sendAbstractMessage(AbstractMessage *message);
    void cancelProcessing();

private:
    absl::Mutex m_lock;
    std::queue<AbstractMessage *> m_queue ABSL_GUARDED_BY(m_lock);
    uv_async_t *m_async;

    friend void on_message_receive(uv_async_t *async);
};

/**
 *
 * @tparam MessageType
 */
template<class MessageType>
class AsyncProcessor : public BaseAsyncProcessor, public AbstractMessageSender<MessageType> {

public:
    typedef bool (*TypedMessageReceivedCallback)(MessageType *, void *);

public:
    AsyncProcessor(TypedMessageReceivedCallback received, void *data);
    void sendMessage(MessageType *message) override;

protected:
    void processAbstractMessage(AbstractMessage *message) override;

private:
    TypedMessageReceivedCallback m_received;
    void *m_data;
};

template<class MessageType>
AsyncProcessor<MessageType>::AsyncProcessor(TypedMessageReceivedCallback received, void *data)
    : BaseAsyncProcessor(),
      m_received(received),
      m_data(data)
{
    TU_ASSERT (m_received != nullptr);
}

template<class MessageType>
void
AsyncProcessor<MessageType>::sendMessage(MessageType *message)
{
    sendAbstractMessage(message);
}

template<class MessageType>
void
AsyncProcessor<MessageType>::processAbstractMessage(AbstractMessage *message)
{
    if (!m_received(static_cast<MessageType *>(message), m_data)) {
        cancelProcessing();
    }
}

#endif // CHORD_LOCAL_MACHINE_ASYNC_PROCESSOR_H
