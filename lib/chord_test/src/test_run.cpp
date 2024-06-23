
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

chord_test::SpawnMachine::SpawnMachine()
{
}

chord_test::SpawnMachine::SpawnMachine(
    std::shared_ptr<lyric_test::AbstractTester> tester,
    const lyric_common::AssemblyLocation &mainLocation,
    const chord_sandbox::MachineExit &machineExit)
    : m_tester(tester),
      m_location(mainLocation),
      m_exit(machineExit)
{
}

chord_test::SpawnMachine::SpawnMachine(const SpawnMachine &other)
    : m_tester(other.m_tester),
      m_location(other.m_location),
      m_exit(other.m_exit)
{
}

std::shared_ptr<lyric_test::AbstractTester>
chord_test::SpawnMachine::getTester() const
{
    return m_tester;
}

lyric_common::AssemblyLocation
chord_test::SpawnMachine::getMainLocation() const
{
    return m_location;
}

chord_sandbox::MachineExit
chord_test::SpawnMachine::getMachineExit() const
{
    return m_exit;
}
