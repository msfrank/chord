#include <gtest/gtest.h>

#include <chord_agent/machine_process.h>
#include <chord_agent/machine_supervisor.h>

TEST(MachineProcess, Spawn)
{
    uv_loop_t loop;
    uv_loop_init(&loop);

    MachineSupervisor supervisor(&loop, std::filesystem::current_path(), 5, 5);
    ASSERT_TRUE (supervisor.initialize().isOk());

    auto machineUrl = tempo_utils::Url::fromString("dev.zuri:exec://local.test/");
    tempo_utils::ProcessInvoker invoker(MOCK_PROCESS_EXECUTABLE, {"mock-process"});

    MachineProcess process(machineUrl, invoker, &supervisor);
    auto status = process.spawn(std::filesystem::current_path());
    ASSERT_TRUE (status.isOk());

    ASSERT_TRUE (supervisor.shutdown().isOk());
}