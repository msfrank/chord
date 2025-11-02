
#include <chord_machine/config_utils.h>
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
#include <zuri_packager/package_reader.h>
#include <zuri_packager/packaging_conversions.h>

#include "chord_common/common_conversions.h"

tempo_utils::Status
chord_machine::configure(ChordLocalMachineConfig &chordLocalMachineConfig, int argc, const char *argv[])
{
    tempo_config::StringParser machineNameParser(std::string{});
    tempo_config::PathParser runDirectoryParser(std::filesystem::current_path());
    chord_common::TransportLocationParser supervisorEndpointParser;
    tempo_config::PathParser packageCacheDirectoryParser;
    tempo_config::SeqTParser packageCacheDirectoriesParser(&packageCacheDirectoryParser, {});
    tempo_config::UrlParser expectedPortParser;
    tempo_config::SetTParser expectedPortsParser(&expectedPortParser, {});
    tempo_config::BooleanParser startSuspendedParser(false);
    tempo_config::PathParser pemRootCABundleFileParser(std::filesystem::path{});
    tempo_config::PathParser logFileParser(std::filesystem::path{});
    zuri_packager::PackageSpecifierParser mainPackageParser;
    tempo_config::StringParser mainArgParser;
    tempo_config::SeqTParser mainArgumentsParser(&mainArgParser);

    std::vector<tempo_command::Default> defaults = {
        {"machineName", {}, "the machine url used for registration", "MACHINE-URL"},
        {"runDirectory", {}, "run the machine in the specified directory", "DIR"},
        {"supervisorEndpoint", {}, "register machine using the specified endpoint", "ENDPOINT"},
        {"packageCacheDirectories", {}, "package cache", "DIR"},
        {"expectedPorts", {}, "expected port", "PROTOCOL-URL"},
        {"startSuspended", {}, "start machine in suspended state"},
        {"pemRootCABundleFile", {}, "the root CA certificate bundle used by gRPC", "FILE"},
        {"logFile", {}, "path to log file", "FILE"},
        {"mainPackage", {}, "Main package", "SPECIFIER"},
        {"mainArgs", {}, "List of arguments to pass to the program", "ARGS"},
    };

    std::vector<tempo_command::Grouping> groupings = {
        {"machineName", {"-n", "--machine-name"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"supervisorEndpoint", {"--supervisor-endpoint"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"runDirectory", {"--run-directory"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"packageCacheDirectories", {"-P", "--package-cache"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"expectedPorts", {"--expected-port"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"startSuspended", {"--start-suspended"}, tempo_command::GroupingType::NO_ARGUMENT},
        {"pemRootCABundleFile", {"--ca-bundle"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"logFile", {"--log-file"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"help", {"-h", "--help"}, tempo_command::GroupingType::HELP_FLAG},
        {"version", {"--version"}, tempo_command::GroupingType::VERSION_FLAG},
    };

    std::vector<tempo_command::Mapping> optMappings = {
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "machineName"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "runDirectory"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "supervisorEndpoint"},
        {tempo_command::MappingType::ANY_INSTANCES, "packageCacheDirectories"},
        {tempo_command::MappingType::ANY_INSTANCES, "expectedPorts"},
        {tempo_command::MappingType::TRUE_IF_INSTANCE, "startSuspended"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "pemRootCABundleFile"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "logFile"},
    };

    std::vector<tempo_command::Mapping> argMappings = {
        {tempo_command::MappingType::ONE_INSTANCE, "mainPackage"},
        {tempo_command::MappingType::ANY_INSTANCES, "mainArgs"},
    };

    tempo_command::OptionsHash options;
    tempo_command::ArgumentVector arguments;

    tempo_command::CommandConfig commandConfig = command_config_from_defaults(defaults);

    // parse argv array into a vector of tokens
    auto tokenizeResult = tempo_command::tokenize_argv(argc - 1, &argv[1]);
    if (tokenizeResult.isStatus())
        tempo_command::display_status_and_exit(tokenizeResult.getStatus());
    auto tokens = tokenizeResult.getResult();

    // parse remaining options and arguments
    auto status = tempo_command::parse_completely(tokens, groupings, options, arguments);
    if (status.notOk()) {
        tempo_command::CommandStatus commandStatus;
        if (!status.convertTo(commandStatus))
            return status;
        switch (commandStatus.getCondition()) {
            case tempo_command::CommandCondition::kHelpRequested:
                tempo_command::display_help_and_exit({"chord-machine"}, "Chord machine",
                    {}, groupings, optMappings, argMappings, defaults);
            default:
                return status;
        }
    }

    // convert options to config
    TU_RETURN_IF_NOT_OK (tempo_command::convert_options(options, optMappings, commandConfig));

    // convert arguments to config
    TU_RETURN_IF_NOT_OK (tempo_command::convert_arguments(arguments, argMappings, commandConfig));

    // construct command map
    tempo_config::ConfigMap commandMap(commandConfig);

    TU_LOG_V << "command config:\n" << tempo_command::command_config_to_string(commandConfig);

    // determine the machine name
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.machineName,
        machineNameParser, commandConfig, "machineName"));

    // determine the run directory
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.runDirectory,
        runDirectoryParser, commandConfig, "runDirectory"));

    // determine the supervisor endpoint
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.supervisorEndpoint,
        supervisorEndpointParser, commandConfig, "supervisorEndpoint"));

    // determine the package directories
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.packageCacheDirectories,
        packageCacheDirectoriesParser, commandConfig, "packageCacheDirectories"));

    // determine the expected ports
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.expectedPorts,
        expectedPortsParser, commandConfig, "expectedPorts"));

    // determine start suspended
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.startSuspended,
        startSuspendedParser, commandConfig, "startSuspended"));

    // determine the pem root CA bundle file
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.pemRootCABundleFile,
        pemRootCABundleFileParser, commandConfig, "pemRootCABundleFile"));

    // determine the log file
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.logFile,
        logFileParser, commandConfig, "logFile"));

    // determine the main package
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.mainPackage,
        mainPackageParser, commandConfig, "mainPackage"));

    // determine the main arguments
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(chordLocalMachineConfig.mainArguments,
        mainArgumentsParser, commandConfig, "mainArguments"));

    // set the binder endpoint
    auto binderSocketPath = chordLocalMachineConfig.runDirectory / "cap.sock";
    chordLocalMachineConfig.binderEndpoint = chord_common::TransportLocation::forUnix()
    chordLocalMachineConfig.binderEndpoint = absl::StrCat(
        "unix://", std::filesystem::absolute(binderSocketPath).c_str());

    // set the binder certificate organization
    chordLocalMachineConfig.binderOrganization = "Chord";

    // set the binder certificate organizational unit
    chordLocalMachineConfig.binderOrganizationalUnit = "Chord machine";

    // set the binder CSR filename stem
    chordLocalMachineConfig.binderCsrFilenameStem = tempo_utils::generate_name("machine-XXXXXXXX");

    return {};
}
