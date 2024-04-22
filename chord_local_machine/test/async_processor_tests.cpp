#include <gtest/gtest.h>

#include <chord_local_machine/async_processor.h>
#include <chord_local_machine/interpreter_runner.h>

struct Context {
    std::vector<std::unique_ptr<RunnerRequest>> messages;
};

static bool receive_messages_until_shutdown(RunnerRequest *message, void *data)
{
    auto *context = static_cast<Context *>(data);
    context->messages.emplace_back(message);
    return message->type != RunnerRequest::MessageType::Shutdown;
}

TEST(AsyncProcessor, SendAndReceiveSingleMessageSynchronously)
{
    uv_loop_t loop;
    uv_loop_init(&loop);

    Context context;
    AsyncProcessor<RunnerRequest> processor(receive_messages_until_shutdown, &context);
    processor.initialize(&loop);

    processor.sendMessage(new ShutdownRunner());
    processor.runUntilCancelled();

    ASSERT_EQ (1, context.messages.size());
    auto *message1 = context.messages.front().get();
    ASSERT_EQ (RunnerRequest::MessageType::Shutdown, message1->type);
}

static void single_producer_thread(void *ptr)
{
    auto *processor = static_cast<AsyncProcessor<RunnerRequest> *>(ptr);
    uv_sleep(250);
    processor->sendMessage(new ShutdownRunner());
}

TEST(AsyncProcessor, SendMessageAndReceiveSingleMessageAsync)
{
    uv_loop_t loop;
    uv_loop_init(&loop);

    Context context;
    AsyncProcessor<RunnerRequest> processor(receive_messages_until_shutdown, &context);
    processor.initialize(&loop);

    uv_thread_t tid;
    uv_thread_create(&tid, single_producer_thread, &processor);

    processor.sendMessage(new ShutdownRunner());
    processor.runUntilCancelled();

    ASSERT_EQ (1, context.messages.size());
    auto *message1 = context.messages.front().get();
    ASSERT_EQ (RunnerRequest::MessageType::Shutdown, message1->type);

    uv_thread_join(&tid);
}

static void multi_producer_thread(void *ptr)
{
    auto *processor = static_cast<AsyncProcessor<RunnerRequest> *>(ptr);
    uv_sleep(100);
    processor->sendMessage(new StartRunner());
    uv_sleep(100);
    processor->sendMessage(new InterruptRunner());
    uv_sleep(100);
    processor->sendMessage(new ShutdownRunner());
}

TEST(AsyncProcessor, SendMessageAndReceiveMultipleMessagesAsync)
{
    uv_loop_t loop;
    uv_loop_init(&loop);

    Context context;
    AsyncProcessor<RunnerRequest> processor(receive_messages_until_shutdown, &context);
    processor.initialize(&loop);

    uv_thread_t tid;
    uv_thread_create(&tid, multi_producer_thread, &processor);

    processor.runUntilCancelled();

    ASSERT_EQ (3, context.messages.size());
    auto *message1 = context.messages.at(0).get();
    ASSERT_EQ (RunnerRequest::MessageType::Start, message1->type);
    auto *message2 = context.messages.at(1).get();
    ASSERT_EQ (RunnerRequest::MessageType::Interrupt, message2->type);
    auto *message3 = context.messages.at(2).get();
    ASSERT_EQ (RunnerRequest::MessageType::Shutdown, message3->type);

    uv_thread_join(&tid);
}
