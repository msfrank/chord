/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include <chord_run/chord_run.h>
#include <chord_run/run_result.h>
#include <chord_run/run_package_command.h>
#include <tempo_command/command_config.h>
#include <tempo_command/command_help.h>
#include <tempo_command/command_parser.h>
#include <tempo_command/command_tokenizer.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/container_conversions.h>
#include <tempo_config/workspace_config.h>
#include <tempo_utils/uuid.h>
#include <zuri_packager/packaging_conversions.h>

#include "chord_tooling/chord_config.h"

tempo_utils::Status
chord_run::chord_run(int argc, const char *argv[])
{
    tempo_config::PathParser distributionRootParser(DISTRIBUTION_ROOT);
    tempo_config::StringParser sessionIsolateParser(std::string{});
    tempo_config::StringParser sessionNameParser(std::string{});
    tempo_config::PathParser pemRootCABundleFileParser(std::filesystem::path{});
    tempo_config::BooleanParser colorizeOutputParser(false);
    tempo_config::IntegerParser verboseParser(0);
    tempo_config::IntegerParser quietParser(0);
    tempo_config::BooleanParser silentParser(false);
    zuri_packager::PackageSpecifierParser packageSpecifierParser;
    tempo_config::StringParser mainArgParser;
    tempo_config::SeqTParser mainArgsParser(&mainArgParser);

    std::vector<tempo_command::Default> defaults = {
        {"distributionRoot", distributionRootParser.getDefault(),
            "Specify an alternative distribution root directory", "DIR"},
        {"sessionIsolate", {}, "Spawn session from the specified isolate", "ISOLATE"},
        {"sessionName", {}, "The session name", "NAME"},
        {"pemRootCABundleFile", {}, "The root CA certificate bundle used by gRPC", "FILE"},
        {"colorizeOutput", colorizeOutputParser.getDefault(),
            "Display colorized output"},
        {"verbose", verboseParser.getDefault(),
            "Display verbose output (specify twice for even more verbose output)"},
        {"quiet", quietParser.getDefault(),
            "Display warnings and errors only (specify twice for errors only)"},
        {"silent", silentParser.getDefault(),
            "Suppress all output"},
        {"packageSpecifier", {}, "Main package", "SPECIFIER"},
        {"mainArgs", {}, "List of arguments to pass to the program", "ARGS"},
    };

    const std::vector<tempo_command::Grouping> groupings = {
        {"distributionRoot", {"--distribution-root"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"sessionIsolate", {"-s", "--session-isolate"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"sessionName", {"-n", "--session-name"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"pemRootCABundleFile", {"--ca-bundle"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"colorizeOutput", {"-c", "--colorize"}, tempo_command::GroupingType::NO_ARGUMENT},
        {"verbose", {"-v"}, tempo_command::GroupingType::NO_ARGUMENT},
        {"quiet", {"-q"}, tempo_command::GroupingType::NO_ARGUMENT},
        {"silent", {"-s", "--silent"}, tempo_command::GroupingType::NO_ARGUMENT},
        {"help", {"-h", "--help"}, tempo_command::GroupingType::HELP_FLAG},
        {"version", {"--version"}, tempo_command::GroupingType::VERSION_FLAG},
    };

    const std::vector<tempo_command::Mapping> optMappings = {
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "distributionRoot"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "sessionIsolate"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "sessionName"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "pemRootCABundleFile"},
        {tempo_command::MappingType::TRUE_IF_INSTANCE, "colorizeOutput"},
        {tempo_command::MappingType::COUNT_INSTANCES, "verbose"},
        {tempo_command::MappingType::COUNT_INSTANCES, "quiet"},
        {tempo_command::MappingType::TRUE_IF_INSTANCE, "silent"},
    };

    std::vector<tempo_command::Mapping> argMappings = {
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "packageSpecifier"},
        {tempo_command::MappingType::ANY_INSTANCES, "mainArgs"},
    };

    // parse argv array into a vector of tokens
    tempo_command::TokenVector tokens;
    TU_ASSIGN_OR_RETURN (tokens, tempo_command::tokenize_argv(argc - 1, &argv[1]));

    tempo_command::OptionsHash options;
    tempo_command::ArgumentVector arguments;

    // parse options and arguments
    auto status = tempo_command::parse_completely(tokens, groupings, options, arguments);
    if (status.notOk()) {
        tempo_command::CommandStatus commandStatus;
        if (!status.convertTo(commandStatus))
            return status;
        switch (commandStatus.getCondition()) {
            case tempo_command::CommandCondition::kHelpRequested:
                display_help_and_exit({"chord-run"},
                    "Run a Chord program",
                    {}, groupings, optMappings, argMappings, defaults);
            case tempo_command::CommandCondition::kVersionRequested:
                tempo_command::display_version_and_exit(PROJECT_VERSION);
            default:
                return status;
        }
    }

    // initialize the command config from defaults
    tempo_command::CommandConfig commandConfig = tempo_command::command_config_from_defaults(defaults);

    // convert options to config
    TU_RETURN_IF_NOT_OK (tempo_command::convert_options(options, optMappings, commandConfig));

    // convert arguments to config
    TU_RETURN_IF_NOT_OK (tempo_command::convert_arguments(arguments, argMappings, commandConfig));

    // construct command map
    tempo_config::ConfigMap commandMap(commandConfig);

    // configure logging
    tempo_utils::LoggingConfiguration logging = {
        tempo_utils::SeverityFilter::kDefault,
        false,
    };

    bool silent;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(silent, silentParser,
        commandConfig, "silent"));
    if (silent) {
        logging.severityFilter = tempo_utils::SeverityFilter::kSilent;
    } else {
        int verbose, quiet;
        TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(verbose, verboseParser,
            commandConfig, "verbose"));
        TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(quiet, quietParser,
            commandConfig, "quiet"));
        if (verbose && quiet)
            return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
                "cannot specify both -v and -q");
        if (verbose == 1) {
            logging.severityFilter = tempo_utils::SeverityFilter::kVerbose;
        } else if (verbose > 1) {
            logging.severityFilter = tempo_utils::SeverityFilter::kVeryVerbose;
        }
        if (quiet == 1) {
            logging.severityFilter = tempo_utils::SeverityFilter::kWarningsAndErrors;
        } else if (quiet > 1) {
            logging.severityFilter = tempo_utils::SeverityFilter::kErrorsOnly;
        }
    }

    bool colorizeOutput;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(colorizeOutput, colorizeOutputParser,
        commandConfig, "colorizeOutput"));

    // initialize logging
    tempo_utils::init_logging(logging);

    TU_LOG_V << "command config:\n" << tempo_command::command_config_to_string(commandConfig);

    // determine the distribution root
    std::filesystem::path distributionRoot;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(distributionRoot, distributionRootParser,
        commandConfig, "distributionRoot"));

    // determine the session isolate
    std::string sessionIsolate;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(sessionIsolate, sessionIsolateParser,
        commandConfig, "sessionIsolate"));

    // determine the session name
    std::string sessionName;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(sessionName, sessionNameParser,
        commandConfig, "sessionName"));

    // determine the pem root CA bundle file
    std::filesystem::path pemRootCABundleFile;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(pemRootCABundleFile, pemRootCABundleFileParser,
        commandConfig, "pemRootCABundleFile"));

    //
    zuri_packager::PackageSpecifier packageSpecifier;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(packageSpecifier, packageSpecifierParser,
        commandConfig, "packageSpecifier"));

    //
    std::vector<std::string> mainArgs;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(mainArgs, mainArgsParser,
        commandConfig, "mainArgs"));

    // if distribution root is relative, then make it absolute
    if (!distributionRoot.empty()) {
        if (distributionRoot.is_relative()) {
            auto executableDir = std::filesystem::path(argv[0]).parent_path();
            distributionRoot = executableDir / distributionRoot;
        }
    }

    TU_LOG_V << "using distribution root " << distributionRoot;

    // load chord config
    std::shared_ptr<chord_tooling::ChordConfig> chordConfig;
    TU_ASSIGN_OR_RETURN (chordConfig, chord_tooling::ChordConfig::forUser(
        {}, distributionRoot));

    return run_package_command(sessionIsolate, sessionName, pemRootCABundleFile, packageSpecifier, mainArgs);
}
