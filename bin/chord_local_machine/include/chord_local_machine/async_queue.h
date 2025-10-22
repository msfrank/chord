#ifndef CHORD_MACHINE_ASYNC_QUEUE_H
#define CHORD_MACHINE_ASYNC_QUEUE_H

#include <queue>

#include <absl/synchronization/mutex.h>
#include <uv.h>

#include <tempo_utils/status.h>

#include "abstract_message_sender.h"

namespace chord_machine {
    /**
     *
     */
    class BaseAsyncQueue {
    public:
        BaseAsyncQueue();
        virtual ~BaseAsyncQueue();

        tempo_utils::Status initialize(uv_loop_t *loop);
        bool messagesPending();

    protected:
        void sendAbstractMessage(AbstractMessage *message);
        AbstractMessage *waitForAbstractMessage();
        AbstractMessage *takeAvailableAbstractMessage();

    private:
        absl::Mutex m_lock;
        std::queue<AbstractMessage *> m_queue ABSL_GUARDED_BY(m_lock);
        uv_async_t *m_async;

        friend void on_async_queue_receive(uv_async_t *async);
    };

    /**
     *
     * @tparam MessageType
     */
    template<class MessageType>
    class AsyncQueue : public BaseAsyncQueue, public AbstractMessageSender<MessageType> {
    public:
        AsyncQueue();

        void sendMessage(MessageType *message) override;
        MessageType *waitForMessage();
        MessageType *takeAvailableMessage();
    };

    template<class MessageType>
    AsyncQueue<MessageType>::AsyncQueue()
        : BaseAsyncQueue()
    {
    }

    template<class MessageType>
    void
    AsyncQueue<MessageType>::sendMessage(MessageType *message)
    {
        sendAbstractMessage(message);
    }

    template<class MessageType>
    MessageType *
    AsyncQueue<MessageType>::waitForMessage()
    {
        return static_cast<MessageType *>(waitForAbstractMessage());
    }

    template<class MessageType>
    MessageType *
    AsyncQueue<MessageType>::takeAvailableMessage()
    {
        return static_cast<MessageType *>(takeAvailableAbstractMessage());
    }
}

#endif // CHORD_MACHINE_ASYNC_QUEUE_H
