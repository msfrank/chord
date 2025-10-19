
#include <chord_sandbox/chord_isolate.h>
#include <chord_run/run_package_command.h>
#include <lyric_runtime/bytecode_interpreter.h>
#include <tempo_utils/tempdir_maker.h>
#include <tempo_utils/unicode.h>

tempo_utils::Status
chord_run::run_package_command(
    std::shared_ptr<chord_tooling::ChordConfig> chordConfig,
    const std::string &sessionIsolate,
    const std::string &sessionName,
    const std::filesystem::path &pemRootCABundleFile,
    const zuri_packager::PackageSpecifier &packageSpecifier,
    const std::vector<std::string> &mainArgs)
{
    TU_ASSERT (sessionIsolate)

    std::shared_ptr<chord_sandbox::ChordIsolate> isolate;
    chord_sandbox::ChordIsolate::connect(agentServerName, agentEndpoint,)
    chord_sandbox::SandboxOptions options;
    options.agentEndpoint = agentEndpoint;
    options.agentServerName = agentServerName;
    options.pemRootCABundleFile = pemRootCABundleFile;

    chord_sandbox::ChordIsolate isolate(options);
    TU_RETURN_IF_NOT_OK (isolate.initialize());

    auto mainLocation = packageSpecifier.toUrl();
    tempo_config::ConfigMap config;
    std::vector<chord_sandbox::RequestedPortAndHandler> plugs;

    std::shared_ptr<chord_sandbox::RemoteMachine> machine;
    TU_ASSIGN_OR_RETURN (machine, isolate.spawn("foo", mainLocation, config, plugs));

    chord_sandbox::MachineExit exit;
    TU_ASSIGN_OR_RETURN (exit, machine->runUntilFinished());

    // print the return value
    TU_CONSOLE_OUT << " ---> " << exit.statusCode;

    return {};
}
