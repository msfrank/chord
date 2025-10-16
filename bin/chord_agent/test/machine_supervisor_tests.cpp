#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_agent/machine_process.h>
#include <chord_agent/machine_supervisor.h>
#include <tempo_test/tempo_test.h>

TEST(MachineSupervisor, Initialize) {
    uv_loop_t loop;
    uv_loop_init(&loop);

    MachineSupervisor supervisor(&loop, std::filesystem::current_path(), 5, 5);
    ASSERT_THAT (supervisor.initialize(), tempo_test::IsOk());

    ASSERT_THAT (supervisor.shutdown(), tempo_test::IsOk());
}
