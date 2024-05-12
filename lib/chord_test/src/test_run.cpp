
#include <chord_test/test_run.h>

chord_test::RunMachine::RunMachine()
    : TestComputation()
{
}

chord_test::RunMachine::RunMachine(
    std::shared_ptr<lyric_test::AbstractTester> tester,
    const lyric_build::TargetComputation &computation,
    std::shared_ptr<lyric_build::BuildDiagnostics> diagnostics,
    const chord_sandbox::MachineExit &exit)
    : TestComputation(tester, computation, diagnostics),
      m_exit(exit)
{
}

chord_test::RunMachine::RunMachine(const RunMachine &other)
    : TestComputation(other),
      m_exit(other.m_exit)
{
}

chord_sandbox::MachineExit
chord_test::RunMachine::getMachineExit() const
{
    return m_exit;
}
