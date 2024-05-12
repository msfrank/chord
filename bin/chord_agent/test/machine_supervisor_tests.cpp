#include <gtest/gtest.h>

#include <chord_agent/machine_process.h>
#include <chord_agent/machine_supervisor.h>

TEST(MachineSupervisor, Initialize)
{
    uv_loop_t loop;
    uv_loop_init(&loop);

    MachineSupervisor supervisor(&loop, std::filesystem::current_path(), 5, 5);
    ASSERT_TRUE (supervisor.initialize().isOk());

    ASSERT_TRUE (supervisor.shutdown().isOk());
}
