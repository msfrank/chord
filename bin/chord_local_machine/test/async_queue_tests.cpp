#include <gtest/gtest.h>

#include <chord_local_machine/async_queue.h>
#include <chord_local_machine/interpreter_runner.h>

TEST(AsyncQueue, QueueIsInitiallyEmpty)
{
    uv_loop_t loop;
    uv_loop_init(&loop);

    AsyncQueue<RunnerRequest> queue;
    queue.initialize(&loop);

    ASSERT_FALSE (queue.messagesPending());
}

TEST(AsyncQueue, SendMessageAndReceiveSynchronously)
{
    uv_loop_t loop;
    uv_loop_init(&loop);

    AsyncQueue<RunnerRequest> queue;
    queue.initialize(&loop);

    ASSERT_FALSE (queue.messagesPending());
    queue.sendMessage(new TerminateRunner());
    ASSERT_TRUE (queue.messagesPending());
    auto *message = queue.waitForMessage();
    ASSERT_TRUE (message != nullptr);
    ASSERT_EQ (RunnerRequest::MessageType::Terminate, message->type);
    delete message;
}

static void producer_thread(void *ptr)
{
    auto *queue = static_cast<AsyncQueue<RunnerRequest> *>(ptr);
    uv_sleep(250);
    queue->sendMessage(new TerminateRunner());
}

TEST(AsyncQueue, SendMessageAndReceiveAsync)
{
    uv_loop_t loop;
    uv_loop_init(&loop);

    AsyncQueue<RunnerRequest> queue;
    queue.initialize(&loop);

    uv_thread_t tid;
    uv_thread_create(&tid, producer_thread, &queue);

    ASSERT_FALSE (queue.messagesPending());
    auto *message = queue.waitForMessage();
    ASSERT_TRUE (message != nullptr);
    ASSERT_EQ (RunnerRequest::MessageType::Terminate, message->type);
    delete message;

    uv_thread_join(&tid);
}
