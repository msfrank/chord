
#include <chord_local_machine/config_utils.h>
#include <lyric_common/common_conversions.h>
#include <tempo_command/command_help.h>
#include <tempo_command/command_parser.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/container_conversions.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/log_stream.h>
#include <tempo_utils/posix_result.h>
#include <tempo_utils/tempfile_maker.h>
#include <tempo_utils/url.h>

tempo_utils::Status
configure(ChordLocalMachineConfig &chordLocalMachineConfig, int argc, const char *argv[])
{
    tempo_config::PathParser runDirectoryParser(std::filesystem::current_path());
    tempo_config::PathParser installDirectoryParser(std::filesystem::path{});
    tempo_config::PathParser packageDirectoryParser;
    tempo_config::SeqTParser<std::filesystem::path> packageDirectoriesParser(&packageDirectoryParser, {});
    tempo_config::PathParser pemRootCABundleFileParser(std::filesystem::path{});
    tempo_config::PathParser logFileParser(
        std::filesystem::path(absl::StrCat("chord-local-machine.", getpid(), ".log")));
    tempo_config::UrlParser expectedPortParser;
    tempo_config::SetTParser<tempo_utils::Url> expectedPortsParser(
        &expectedPortParser,
        {});
    tempo_config::UrlParser supervisorUrlParser;
    tempo_config::StringParser supervisorNameOverrideParser(std::string{});
    tempo_config::UrlParser machineUrlParser;
    tempo_config::StringParser machineNameOverrideParser(std::string{});
    lyric_common::AssemblyLocationParser mainLocationParser;

    std::vector<tempo_command::Default> defaults = {
        {"runDirectory", {}, "run directory", "DIR"},
        {"installDirectory", {}, "install directory", "DIR"},
        {"packageDirectories", {}, "package directory", "DIR"},
        {"expectedPorts", {}, "expectedPorts", "PROTOCOL-URL"},
        {"supervisorUrl", {}, "register interpreter using the specified uri", "SUPERVISOR-URL"},
        {"supervisorNameOverride", {}, "the SSL server name of the supervisor", "SERVER-NAME"},
        {"machineUrl", {}, "the machine url used for registration", "MACHINE-URL"},
        {"machineNameOverride", {}, "the SSL server name of the machine", "SERVER-NAME"},
        {"pemRootCABundleFile", {}, "the root CA certificate bundle used by gRPC", "FILE"},
        {"logFile", {}, "path to log file", "FILE"},
        {"mainLocation", {}, "the location of the main module", "LOCATION"},
    };

    std::vector<tempo_command::Grouping> groupings = {
        {"runDirectory", {"--run-directory"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"installDirectory", {"--install-directory"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"packageDirectories", {"--package-directory"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"expectedPorts", {"--expected-port"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"supervisorNameOverride", {"--supervisor-server-name"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"machineNameOverride", {"--machine-server-name"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"pemRootCABundleFile", {"--ca-bundle"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"logFile", {"--log-file"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"help", {"-h", "--help"}, tempo_command::GroupingType::HELP_FLAG},
        {"version", {"--version"}, tempo_command::GroupingType::VERSION_FLAG},
    };

    std::vector<tempo_command::Mapping> optMappings = {
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "runDirectory"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "installDirectory"},
        {tempo_command::MappingType::ANY_INSTANCES, "packageDirectories"},
        {tempo_command::MappingType::ANY_INSTANCES, "expectedPorts"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "supervisorUrl"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "machineUrl"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "supervisorNameOverride"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "machineNameOverride"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "pemRootCABundleFile"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "logFile"},
    };

    std::vector<tempo_command::Mapping> argMappings = {
        {tempo_command::MappingType::ONE_INSTANCE, "supervisorUrl"},
        {tempo_command::MappingType::ONE_INSTANCE, "mainLocation"},
        {tempo_command::MappingType::ONE_INSTANCE, "machineUrl"},
    };

    tempo_command::OptionsHash options;
    tempo_command::ArgumentVector arguments;

    tempo_command::CommandConfig config = command_config_from_defaults(defaults);

    // parse argv array into a vector of tokens
    auto tokenizeResult = tempo_command::tokenize_argv(argc - 1, &argv[1]);
    if (tokenizeResult.isStatus())
        display_status_and_exit(tokenizeResult.getStatus());
    auto tokens = tokenizeResult.getResult();

    // parse remaining options and arguments
    auto status = tempo_command::parse_completely(tokens, groupings, options, arguments);
    if (status.notOk()) {
        tempo_command::CommandStatus commandStatus;
        if (!status.convertTo(commandStatus))
            return status;
        switch (commandStatus.getCondition()) {
            case tempo_command::CommandCondition::kHelpRequested:
                tempo_command::display_help_and_exit({"chord-local-machine"}, "Chord local machine",
                    {}, groupings, optMappings, argMappings, defaults);
            default:
                return status;
        }
    }

    // convert options to config
    status = convert_options(options, optMappings, config);
    if (!status.isOk())
        return status;

    // convert arguments to config
    status = convert_arguments(arguments, argMappings, config);
    if (!status.isOk())
        return status;

    TU_LOG_INFO << "config:\n" << tempo_command::command_config_to_string(config);

    // determine the run directory
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.runDirectory,
        runDirectoryParser, config, "runDirectory"));

    // determine the install directory
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.installDirectory,
        installDirectoryParser, config, "installDirectory"));

    // determine the package directories
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.packageDirectories,
        packageDirectoriesParser, config, "packageDirectories"));

    // determine the expected ports
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.expectedPorts,
        expectedPortsParser, config, "expectedPorts"));

    // determine the supervisor uri
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.supervisorUrl,
        supervisorUrlParser, config, "supervisorUrl"));

    // determine the SSL server name of the supervisor
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.supervisorNameOverride,
        supervisorNameOverrideParser, config, "supervisorNameOverride"));

    // determine the machine uri
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.machineUrl,
        machineUrlParser, config, "machineUrl"));

    // determine the SSL server name of the supervisor
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.machineNameOverride,
        machineNameOverrideParser, config, "machineNameOverride"));

    // determine the pem root CA bundle file
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.pemRootCABundleFile,
        pemRootCABundleFileParser, config, "pemRootCABundleFile"));

    // determine the log file
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.logFile,
        logFileParser, config, "logFile"));

    // determine the location of the main assembly
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.mainLocation,
        mainLocationParser, config, "mainLocation"));


    // set the binder endpoint
    auto binderSocketPath = chordLocalMachineConfig.runDirectory / "cap.sock";
    chordLocalMachineConfig.binderEndpoint = absl::StrCat(
        "unix://", std::filesystem::absolute(binderSocketPath).c_str());

    // set the binder certificate organization
    chordLocalMachineConfig.binderOrganization = "Chord";

    // set the binder certificate organizational unit
    chordLocalMachineConfig.binderOrganizationalUnit = "Chord local machine";

    // set the binder CSR filename stem
    chordLocalMachineConfig.binderCsrFilenameStem = tempo_utils::generate_name("machine-XXXXXXXX");

    return {};
}
