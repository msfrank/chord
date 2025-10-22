
#include <signal.h>

#include <chord_machine/chord_machine.h>
#include <tempo_command/command_help.h>

static const char *SUSPEND_ON_STARTUP = "SUSPEND_ON_STARTUP";

int
main(int argc, const char *argv[])
{
    if (argc == 0 || argv == nullptr)
        return -1;

    // stop the process, wait for SIGCONT
    if (getenv(SUSPEND_ON_STARTUP)) {
        kill(getpid(), SIGSTOP);
    }

    auto status = chord_machine::chord_machine(argc, argv);
    if (!status.isOk()) {
        tempo_command::display_status_and_exit(status);
    }
    return 0;
}
