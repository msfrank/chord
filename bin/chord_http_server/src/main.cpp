
#include <signal.h>

#include <chord_http_server/chord_http_server.h>
#include <tempo_command/command_help.h>

static const char *SUSPEND_ON_STARTUP = "SUSPEND_ON_STARTUP";

int
main(int argc, char *argv[])
{
    if (argc == 0 || argv == nullptr)
        return -1;

    // stop the process, wait for SIGCONT
    if (getenv(SUSPEND_ON_STARTUP)) {
        kill(getpid(), SIGSTOP);
    }

    auto status = run_chord_http_server(argc, argv);
    if (!status.isOk()) {
        TU_LOG_V << status;
        tempo_command::display_status_and_exit(status);
    }
    return 0;
}