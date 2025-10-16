#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_agent/machine_process.h>
#include <chord_agent/machine_supervisor.h>
#include <tempo_test/tempo_test.h>

TEST(MachineProcess, Spawn) {
    uv_loop_t loop;
    uv_loop_init(&loop);

    MachineSupervisor supervisor(&loop, std::filesystem::current_path(), 5, 5);
    ASSERT_THAT (supervisor.initialize(), tempo_test::IsOk());

    auto machineUrl = tempo_utils::Url::fromString("dev.zuri:exec://local.test/");
    tempo_utils::ProcessInvoker invoker(MOCK_PROCESS_EXECUTABLE, {"mock-process"});

    MachineProcess process(machineUrl, invoker, &supervisor);
    ASSERT_THAT (process.spawn(std::filesystem::current_path()), tempo_test::IsOk());

    ASSERT_THAT (supervisor.shutdown(), tempo_test::IsOk());
}