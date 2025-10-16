#ifndef CHORD_TEST_TEST_RUN_H
#define CHORD_TEST_TEST_RUN_H

#include <chord_sandbox/remote_machine.h>
#include <lyric_test/abstract_tester.h>
#include <lyric_test/test_run.h>

namespace chord_test {

    class RunMachine : public lyric_test::TestComputation {

    public:
        RunMachine();
        RunMachine(
            std::shared_ptr<lyric_test::AbstractTester> tester,
            const lyric_build::TargetComputation &computation,
            std::shared_ptr<lyric_build::BuildDiagnostics> diagnostics,
            const chord_sandbox::MachineExit &machineExit);
        RunMachine(const RunMachine &other);

        chord_sandbox::MachineExit getMachineExit() const;

    private:
        chord_sandbox::MachineExit m_exit;
    };

    class SpawnMachine {

    public:
        SpawnMachine();
        SpawnMachine(
            std::shared_ptr<lyric_test::AbstractTester> tester,
            const tempo_utils::Url &mainLocation,
            const chord_sandbox::MachineExit &machineExit);
        SpawnMachine(const SpawnMachine &other);

        std::shared_ptr<lyric_test::AbstractTester> getTester() const;
        tempo_utils::Url getMainLocation() const;
        chord_sandbox::MachineExit getMachineExit() const;

    private:
        std::shared_ptr<lyric_test::AbstractTester> m_tester;
        tempo_utils::Url m_location;
        chord_sandbox::MachineExit m_exit;
    };
}

#endif // CHORD_TEST_TEST_RUN_H
