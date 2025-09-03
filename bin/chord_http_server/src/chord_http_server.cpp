#include <vector>

#include <boost/beast.hpp>
#include <grpcpp/server.h>
#include <uv.h>

#include <chord_http_server/http_service.h>
#include <tempo_command/command_help.h>
#include <tempo_command/command_parser.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/enum_conversions.h>
#include <tempo_utils/file_reader.h>
#include <tempo_utils/log_sink.h>
#include <tempo_utils/log_stream.h>
#include <tempo_utils/posix_result.h>

static void
on_termination_signal(uv_signal_t *handle, int signal)
{
    auto *server = static_cast<chord_http_server::HttpService *>(handle->data);
    TU_LOG_INFO << "caught signal " << signal;
    TU_RAISE_IF_NOT_OK (server->shutdown());
    uv_stop(handle->loop);
}

tempo_utils::Status
run_chord_http_server(int argc, char *argv[])
{
    char hostname[256];
    memset(hostname, 0, 256);
    size_t len = 256;
    uv_os_gethostname(hostname, &len);
    auto processName = absl::StrCat(getpid(), "@", hostname);

    tempo_config::StringParser listenEndpointParser(std::string{});
    tempo_config::PathParser processRunDirectoryParser(std::filesystem::current_path());
    tempo_config::PathParser pemCertificateFileParser(std::filesystem::path{});
    tempo_config::PathParser pemPrivateKeyFileParser(std::filesystem::path{});
    tempo_config::PathParser pemRootCABundleFileParser(std::filesystem::path{});
    tempo_config::BooleanParser runInBackgroundParser(false);
    tempo_config::BooleanParser emitEndpointParser(false);
    //tempo_config::PathParser logFileParser(std::filesystem::path(absl::StrCat("chord-http-server.", getpid(), ".log")));
    tempo_config::PathParser logFileParser(std::filesystem::path{});
    tempo_config::PathParser pidFileParser(std::filesystem::path{});

    std::vector<tempo_command::Default> defaults = {
        {"listenEndpoint", {}, "listen on the specified endpoint", "ENDPOINT"},
        {"processRunDirectory", processRunDirectoryParser.getDefault(),
            "listen on the specified endpoint url", "DIR"},
        {"pemCertificateFile", {}, "the certificate used by gRPC", "FILE"},
        {"pemPrivateKeyFile", {}, "the private key used by gRPC", "FILE"},
        {"pemRootCABundleFile", {}, "the root CA certificate bundle used by gRPC", "FILE"},
        {"runInBackground", {}, "run agent in the background", {}},
        {"emitEndpoint", {}, "print the endpoint url after initialization has completed", {}},
        {"logFile", {}, "path to log file", "FILE"},
        {"pidFile", {}, "record the agent process id in the specified pid file", "FILE"},
    };

    std::vector<tempo_command::Grouping> groupings = {
        {"listenEndpoint", {"-l", "--listen-endpoint"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"pemCertificateFile", {"--certificate"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"pemPrivateKeyFile", {"--private-key"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"pemRootCABundleFile", {"--ca-bundle"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"runInBackground", {"--background"}, tempo_command::GroupingType::NO_ARGUMENT},
        {"emitEndpoint", {"--emit-endpoint"}, tempo_command::GroupingType::NO_ARGUMENT},
        {"logFile", {"--log-file"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"pidFile", {"--pid-file"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"help", {"-h", "--help"}, tempo_command::GroupingType::HELP_FLAG},
        {"version", {"--version"}, tempo_command::GroupingType::VERSION_FLAG},
    };

    std::vector<tempo_command::Mapping> optMappings = {
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "listenEndpoint"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "pemCertificateFile"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "pemPrivateKeyFile"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "pemRootCABundleFile"},
        {tempo_command::MappingType::TRUE_IF_INSTANCE, "runInBackground"},
        {tempo_command::MappingType::TRUE_IF_INSTANCE, "emitEndpoint"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "logFile"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "pidFile"},
    };

    std::vector<tempo_command::Mapping> argMappings = {
    };

    tempo_command::OptionsHash options;
    tempo_command::ArgumentVector arguments;

    tempo_command::CommandConfig commandConfig = command_config_from_defaults(defaults);

    // parse argv array into a vector of tokens
    auto tokenizeResult = tempo_command::tokenize_argv(argc - 1, (const char **) &argv[1]);
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
                tempo_command::display_help_and_exit({"chord-agent"}, "Chord agent",
                    {}, groupings, optMappings, argMappings, defaults);
            default:
                return status;
        }
    }

    // convert options to config
    TU_RETURN_IF_NOT_OK (tempo_command::convert_options(options, optMappings, commandConfig));

    // convert arguments to config
    TU_RETURN_IF_NOT_OK (tempo_command::convert_arguments(arguments, argMappings, commandConfig));

    TU_LOG_INFO << "config:\n" << tempo_command::command_config_to_string(commandConfig);

    // determine the listen endpoint
    std::string listenEndpoint;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(listenEndpoint, listenEndpointParser,
        commandConfig, "listenEndpoint"));

    // determine the process run directory
    std::filesystem::path processRunDirectory;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(processRunDirectory, processRunDirectoryParser,
        commandConfig, "processRunDirectory"));

    // determine the pem certificate file
    std::filesystem::path pemCertificateFile;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(pemCertificateFile, pemCertificateFileParser,
        commandConfig, "pemCertificateFile"));

    // determine the pem private key file
    std::filesystem::path pemPrivateKeyFile;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(pemPrivateKeyFile, pemPrivateKeyFileParser,
        commandConfig, "pemPrivateKeyFile"));

    // determine the pem root CA bundle file
    std::filesystem::path pemRootCABundleFile;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(pemRootCABundleFile, pemRootCABundleFileParser,
        commandConfig, "pemRootCABundleFile"));

    // parse the run in background flag
    bool runInBackground;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(runInBackground, runInBackgroundParser,
        commandConfig, "runInBackground"));

    // parse the emit endpoint flag
    bool emitEndpoint;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(emitEndpoint, emitEndpointParser,
        commandConfig, "emitEndpoint"));

    // determine the log file
    std::filesystem::path logFile;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(logFile, logFileParser,
        commandConfig, "logFile"));

    // determine the pid file
    std::filesystem::path pidFile;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(pidFile, pidFileParser,
        commandConfig, "pidFile"));

    // initialize logging
    tempo_utils::LoggingConfiguration loggingConfig;
    loggingConfig.severityFilter = tempo_utils::SeverityFilter::kVeryVerbose;
    loggingConfig.flushEveryMessage = true;
    if (!logFile.empty()) {
        auto logSink = std::make_unique<tempo_utils::LogFileSink>(logFile);
        tempo_utils::init_logging(loggingConfig, std::move(logSink));
    } else {
        tempo_utils::init_logging(loggingConfig);
    }

    // if agent should run in the background, then fork and continue in the child
    if (runInBackground) {
        auto pid = fork();
        if (pid < 0)
            return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
                "failed to fork into the background");
        if (pid > 0) {
            TU_LOG_INFO << "forked agent into the background with pid " << pid;
            _exit(0);
        }
        auto sid = setsid();
        if (sid < 0)
            return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
                "failed to set session");
        TU_LOG_INFO << "set session sid " << sid;
    }

    // TODO: if pid file is specified, then write the pid file

    //
    auto server = std::make_unique<chord_http_server::HttpService>(4);

    // initialize uv loop
    uv_loop_t loop;
    uv_loop_init(&loop);

    // catch SIGTERM indicating request to cleanly shutdown
    uv_signal_t sigterm;
    uv_signal_init(&loop, &sigterm);
    sigterm.data = server.get();
    uv_signal_start_oneshot(&sigterm, on_termination_signal, SIGTERM);

    // catch SIGINT indicating request to cleanly shutdown
    uv_signal_t sigint;
    uv_signal_init(&loop, &sigint);
    sigint.data = server.get();
    uv_signal_start_oneshot(&sigint, on_termination_signal, SIGINT);

    auto address = boost::asio::ip::make_address("127.0.0.1");
    boost::asio::ip::tcp::endpoint endpoint(address, 8080);

    //
    TU_RETURN_IF_NOT_OK (server->initialize(endpoint));

    //
    TU_RETURN_IF_NOT_OK (server->run());

    // run main loop waiting for a termination signal
    TU_LOG_V << "entering main loop";
    auto ret = uv_run(&loop, UV_RUN_DEFAULT);
    TU_LOG_V << "exiting main loop";
    if (ret < 0)
        return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
            "failed to run main loop");

    TU_LOG_V << "closing main loop";
    uv_close((uv_handle_t *) &sigterm, nullptr);
    uv_close((uv_handle_t *) &sigint, nullptr);

    //
    for (int i = 0; i < 5; i++) {
        ret = uv_loop_close(&loop);
        if (ret == 0)
            break;
        if (ret != UV_EBUSY)
            break;
        uv_run(&loop, UV_RUN_NOWAIT);
    }

    if (ret != 0) {
        TU_LOG_WARN << "failed to close main loop: " << uv_strerror(ret);
        uv_print_all_handles(&loop, stderr);
    }

    // release the service
    server.reset();

    return {};
}