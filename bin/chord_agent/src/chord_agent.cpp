#include <vector>

#include <grpcpp/server.h>
#include <uv.h>

#include <chord_agent/agent_service.h>
#include <chord_agent/machine_supervisor.h>
#include <chord_common/protocol_types.h>
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
    TU_LOG_INFO << "caught signal " << signal;
    uv_stop(handle->loop);
}

static tempo_utils::Result<std::shared_ptr<grpc::ServerCredentials>>
make_ssl_server_credentials(
    const std::filesystem::path &pemCertificateFile,
    const std::filesystem::path &pemPrivateKeyFile,
    const std::filesystem::path &pemRootCABundleFile)
{
    tempo_utils::FileReader certificateReader(pemCertificateFile);
    TU_RETURN_IF_NOT_OK (certificateReader.getStatus());
    tempo_utils::FileReader privateKeyReader(pemPrivateKeyFile);
    TU_RETURN_IF_NOT_OK (privateKeyReader.getStatus());
    tempo_utils::FileReader rootCABundleReader(pemRootCABundleFile);
    TU_RETURN_IF_NOT_OK (rootCABundleReader.getStatus());

    grpc::SslServerCredentialsOptions options;
    grpc::SslServerCredentialsOptions::PemKeyCertPair pair;
    auto rootCABytes = rootCABundleReader.getBytes();
    auto certificateBytes = certificateReader.getBytes();
    auto privateKeyBytes = privateKeyReader.getBytes();
    options.pem_root_certs = std::string((const char *) rootCABytes->getData(), rootCABytes->getSize());
    pair.cert_chain = std::string((const char *) certificateBytes->getData(), certificateBytes->getSize());
    pair.private_key = std::string((const char *) privateKeyBytes->getData(), privateKeyBytes->getSize());
    options.pem_key_cert_pairs.push_back(pair);

    return grpc::SslServerCredentials(options);
}

tempo_utils::Status
run_chord_agent(int argc, const char *argv[])
{
    char hostname[256];
    memset(hostname, 0, 256);
    size_t len = 256;
    uv_os_gethostname(hostname, &len);
    auto processName = absl::StrCat(getpid(), "@", hostname);

    tempo_config::StringParser agentNameParser;
    tempo_config::StringParser listenEndpointParser(std::string{});
    tempo_config::EnumTParser<chord_common::TransportType> listenTransportParser({
        {"unix", chord_common::TransportType::Unix},
        {"tcp", chord_common::TransportType::Tcp},
    }, chord_common::TransportType::Unix);
    tempo_config::PathParser processRunDirectoryParser(std::filesystem::current_path());
    tempo_config::PathParser localMachineExecutableParser(std::filesystem::path(CHORD_LOCAL_MACHINE_EXECUTABLE));
    tempo_config::PathParser pemCertificateFileParser(std::filesystem::path{});
    tempo_config::PathParser pemPrivateKeyFileParser(std::filesystem::path{});
    tempo_config::PathParser pemRootCABundleFileParser(std::filesystem::path{});
    tempo_config::BooleanParser runInBackgroundParser(false);
    tempo_config::BooleanParser temporarySessionParser(false);
    tempo_config::IntegerParser idleTimeoutParser(0);
    tempo_config::IntegerParser registrationTimeoutParser(5);
    tempo_config::PathParser logFileParser(std::filesystem::path(absl::StrCat("chord-agent.", getpid(), ".log")));
    tempo_config::PathParser pidFileParser(std::filesystem::path{});

    std::vector<tempo_command::Default> defaults = {
        {"agentName", {}, "the agent name", "NAME"},
        {"listenEndpoint", {}, "listen on the specified endpoint", "ENDPOINT"},
        {"listenTransport", {}, "use the specified listener transport type", "TYPE"},
        {"processRunDirectory", processRunDirectoryParser.getDefault(),
            "run the agent in the specified directory", "DIR"},
        {"localMachineExecutable", localMachineExecutableParser.getDefault(),
            "path to the local machine executable", "PATH"},
        {"pemCertificateFile", {}, "the certificate used by gRPC", "FILE"},
        {"pemPrivateKeyFile", {}, "the private key used by gRPC", "FILE"},
        {"pemRootCABundleFile", {}, "the root CA certificate bundle used by gRPC", "FILE"},
        {"runInBackground", {}, "run agent in the background", {}},
        {"temporarySession", {}, "agent will shutdown automatically after a period of inactivity", {}},
        {"idleTimeout", {}, "shutdown the agent after the specified amount of time has elapsed", "SECONDS"},
        {"registrationTimeout", registrationTimeoutParser.getDefault(),
            "abandon the execution if not registered after the specified amount of time has elapsed", "SECONDS"},
        {"logFile", {}, "path to log file", "FILE"},
        {"pidFile", {}, "record the agent process id in the specified pid file", "FILE"},
    };

    std::vector<tempo_command::Grouping> groupings = {
        {"agentName", {"--agent-name"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"listenEndpoint", {"-l", "--listen-endpoint"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"listenTransport", {"-t", "--listen-transport"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"processRunDirectory", {"-d", "--run-directory"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"pemCertificateFile", {"--certificate"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"pemPrivateKeyFile", {"--private-key"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"pemRootCABundleFile", {"--ca-bundle"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"runInBackground", {"--background"}, tempo_command::GroupingType::NO_ARGUMENT},
        {"temporarySession", {"--temporary-session"}, tempo_command::GroupingType::NO_ARGUMENT},
        {"idleTimeout", {"--idle-timeout"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"registrationTimeout", {"--registration-timeout"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"logFile", {"--log-file"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"pidFile", {"--pid-file"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"help", {"-h", "--help"}, tempo_command::GroupingType::HELP_FLAG},
        {"version", {"--version"}, tempo_command::GroupingType::VERSION_FLAG},
    };

    std::vector<tempo_command::Mapping> optMappings = {
        {tempo_command::MappingType::ONE_INSTANCE, "agentName"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "listenEndpoint"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "listenTransport"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "processRunDirectory"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "pemCertificateFile"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "pemPrivateKeyFile"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "pemRootCABundleFile"},
        {tempo_command::MappingType::TRUE_IF_INSTANCE, "runInBackground"},
        {tempo_command::MappingType::TRUE_IF_INSTANCE, "temporarySession"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "idleTimeout"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "registrationTimeout"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "logFile"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "pidFile"},
    };

    std::vector<tempo_command::Mapping> argMappings = {
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

    TU_LOG_INFO << "command config:\n" << tempo_command::command_config_to_string(commandConfig);

    // determine the agent name
    std::string agentName;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(agentName, agentNameParser,
        commandConfig, "agentName"));

    // determine the listen endpoint
    std::string listenEndpoint;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(listenEndpoint, listenEndpointParser,
        commandConfig, "listenEndpoint"));

    // determin the listen transport
    chord_common::TransportType listenTransport;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(listenTransport, listenTransportParser,
        commandConfig, "listenTransport"));

    // determine the process run directory
    std::filesystem::path processRunDirectory;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(processRunDirectory, processRunDirectoryParser,
        commandConfig, "processRunDirectory"));

    // determine the local machine executable
    std::filesystem::path localMachineExecutable;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(localMachineExecutable, localMachineExecutableParser,
        commandConfig, "localMachineExecutable"));

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

    // parse the temporary session flag
    bool temporarySession;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(temporarySession, temporarySessionParser,
        commandConfig, "temporarySession"));

    // parse the idle timeout option
    int idleTimeout;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(idleTimeout, idleTimeoutParser,
        commandConfig, "idleTimeout"));

    // parse the registration timeout option
    int registrationTimeout;
    TU_RETURN_IF_NOT_OK(tempo_command::parse_command_config(registrationTimeout, registrationTimeoutParser,
        commandConfig, "registrationTimeout"));

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
    bool usingDefaultLogSink = true;
    if (!logFile.empty()) {
        auto logSink = std::make_unique<tempo_utils::LogFileSink>(logFile);
        tempo_utils::init_logging(loggingConfig, std::move(logSink));
        usingDefaultLogSink = false;
    } else {
        tempo_utils::init_logging(loggingConfig);
    }

    // construct the listener url
    std::string listenerUrl;
    switch (listenTransport) {
        case chord_common::TransportType::Unix: {
            if (listenEndpoint.empty()) {
                auto path = std::filesystem::absolute(processRunDirectory / "agent.sock");
                listenerUrl = absl::StrCat("unix:", path.string());
            } else {
                listenerUrl = absl::StrCat("unix:", listenEndpoint);
            }
            break;
        }
        case chord_common::TransportType::Tcp: {
            if (listenEndpoint.empty()) {
                listenerUrl = "dns:localhost";
            } else {
                listenerUrl = absl::StrCat("dns:", listenEndpoint);
            }
            break;
        }
        default:
            return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
                "unknown transport type");
    }

    // if agent should run in the background, then fork and continue in the child
    if (runInBackground) {

        //
        if (usingDefaultLogSink) {
            tempo_utils::cleanup_logging();
        }

        if (fclose(stdout) < 0)
            return tempo_utils::PosixStatus::last("failed to close stdout");
        if (fclose(stderr) < 0)
            return tempo_utils::PosixStatus::last("failed to close stderr");

        //
        auto pid = fork();
        if (pid < 0)
            return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
                "failed to fork into the background");
        if (pid > 0) {
            TU_LOG_INFO << "forked agent into the background with pid " << pid;
            return {};
        }
        auto sid = setsid();
        if (sid < 0)
            return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
                "failed to set session");
        TU_LOG_INFO << "set session sid " << sid;
    }

    // TODO: if pid file is specified, then write the pid file

    // initialize uv loop
    uv_loop_t loop;
    uv_loop_init(&loop);

    // construct and initialize the supervisor
    MachineSupervisor supervisor(&loop, processRunDirectory, idleTimeout, registrationTimeout);
    TU_RETURN_IF_NOT_OK (supervisor.initialize());

    // construct the agent service
    AgentService service(listenerUrl, &supervisor, agentName, localMachineExecutable);

    // construct the server and start it up
    grpc::ServerBuilder builder;
    std::shared_ptr<grpc::ServerCredentials> credentials;
    TU_ASSIGN_OR_RETURN (credentials, make_ssl_server_credentials(pemCertificateFile,
        pemPrivateKeyFile, pemRootCABundleFile));
    builder.AddListeningPort(listenerUrl, credentials);
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    TU_LOG_INFO << "starting service using endpoint " << listenerUrl;

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

    // redirect stdout to null
    int nullfd = open("/dev/null", O_WRONLY | O_EXCL | O_CLOEXEC);
    if (nullfd < 0)
        return tempo_utils::PosixStatus::last("failed to open /dev/null");
    dup2(nullfd, STDOUT_FILENO);
    close(nullfd);

    // run main loop waiting for a termination signal
    TU_LOG_V << "entering main loop";
    auto ret = uv_run(&loop, UV_RUN_DEFAULT);
    TU_LOG_V << "exiting main loop";
    if (ret < 0)
        return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
            "failed to run main loop");

    TU_LOG_V << "shutting down supervisor";
    supervisor.shutdown();

    TU_LOG_V << "closing main loop";
    uv_print_all_handles(&loop, stderr);

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

    //
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds{5};
    server->Shutdown(deadline);

    return {};
}