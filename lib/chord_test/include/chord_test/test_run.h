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
}

#endif // CHORD_TEST_TEST_RUN_H
