#include <vector>

#include <grpcpp/server.h>
#include <uv.h>

#include <chord_agent/agent_service.h>
#include <chord_agent/chord_agent.h>
#include <chord_agent/machine_supervisor.h>
#include <chord_common/transport_location.h>
#include <tempo_command/command_help.h>
#include <tempo_command/command_parser.h>
#include <tempo_config/base_conversions.h>
#include <tempo_config/enum_conversions.h>
#include <tempo_security/x509_certificate.h>
#include <tempo_utils/file_reader.h>
#include <tempo_utils/file_writer.h>
#include <tempo_utils/log_sink.h>
#include <tempo_utils/log_stream.h>
#include <tempo_utils/posix_result.h>

#include "chord_agent/agent_config.h"
#include "chord_common/common_conversions.h"

static void
on_termination_signal(uv_signal_t *handle, int signal)
{
    TU_LOG_INFO << "caught signal " << signal;
    uv_stop(handle->loop);
}

static tempo_utils::Result<std::shared_ptr<grpc::ServerCredentials>>
make_ssl_server_credentials(const chord_agent::AgentConfig &agentConfig)
{
    tempo_utils::FileReader certificateReader(agentConfig.pemCertificateFile);
    TU_RETURN_IF_NOT_OK (certificateReader.getStatus());
    tempo_utils::FileReader privateKeyReader(agentConfig.pemPrivateKeyFile);
    TU_RETURN_IF_NOT_OK (privateKeyReader.getStatus());
    tempo_utils::FileReader rootCABundleReader(agentConfig.pemRootCABundleFile);
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
chord_agent::chord_agent(int argc, const char *argv[])
{
    std::vector<tempo_command::Default> defaults = {
        {"sessionName", {}, "the session name", "NAME"},
        {"listenEndpoint", {}, "listen on the specified endpoint", "ENDPOINT"},
        {"listenTransport", {}, "autoselect the listen endpoint using the specified transport type", "TYPE"},
        {"endpointFile", {}, "record the listen endpoint in the specified endpoint file", "FILE"},
        {"runDirectory", {}, "run the agent in the specified directory", "DIR"},
        {"pemCertificateFile", {}, "the certificate used by gRPC", "FILE"},
        {"pemPrivateKeyFile", {}, "the private key used by gRPC", "FILE"},
        {"pemRootCABundleFile", {}, "the root CA certificate bundle used by gRPC", "FILE"},
        {"runInBackground", {}, "run agent in the background", {}},
        {"temporarySession", {}, "agent will shutdown automatically after a period of inactivity", {}},
        {"idleTimeout", {}, "shutdown the agent after the specified amount of time has elapsed", "SECONDS"},
        {"registrationTimeout", {}, "abandon the execution if not registered after the specified amount of time has elapsed", "SECONDS"},
        {"logFile", {}, "path to log file", "FILE"},
        {"pidFile", {}, "record the agent process id in the specified pid file", "FILE"},
    };

    std::vector<tempo_command::Grouping> groupings = {
        {"sessionName", {"-n", "--session-name"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"listenEndpoint", {"-l", "--listen-endpoint"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"listenTransport", {"-t", "--listen-transport"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"endpointFile", {"-e", "--endpoint-file"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
        {"runDirectory", {"-d", "--run-directory"}, tempo_command::GroupingType::SINGLE_ARGUMENT},
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
        {tempo_command::MappingType::ONE_INSTANCE, "sessionName"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "listenEndpoint"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "listenTransport"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "endpointFile"},
        {tempo_command::MappingType::ZERO_OR_ONE_INSTANCE, "runDirectory"},
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
    tempo_command::CommandConfig commandConfig;

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

    AgentConfig agentConfig;
    TU_RETURN_IF_NOT_OK (configure_agent(commandConfig, agentConfig));

    // if agent should run in the background, then fork and continue in the child
    if (agentConfig.runInBackground) {

        // ensure stdout and stderr are closed before forking
        if (fclose(stdout) < 0)
            return tempo_utils::PosixStatus::last("failed to close stdout");
        if (fclose(stderr) < 0)
            return tempo_utils::PosixStatus::last("failed to close stderr");

        // fork
        auto pid = fork();
        if (pid < 0)
            return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
                "failed to fork into the background");
        // exit the parent process
        if (pid > 0) {
            TU_LOG_INFO << "forked agent into the background with pid " << pid;
            return {};
        }

        // useful for debugging the child process
        //kill(getpid(), SIGSTOP);

        // make child process the session leader
        auto sid = setsid();
        if (sid < 0)
            return tempo_command::CommandStatus::forCondition(tempo_command::CommandCondition::kCommandError,
                "failed to set session");
        TU_LOG_INFO << "set session sid " << sid;
    }

    tempo_utils::LoggingConfiguration loggingConfig;
    loggingConfig.severityFilter = tempo_utils::SeverityFilter::kVeryVerbose;
    loggingConfig.flushEveryMessage = true;

    // initialize logging
    if (!agentConfig.logFile.empty()) {
        auto logSink = std::make_unique<tempo_utils::LogFileSink>(agentConfig.logFile);
        tempo_utils::init_logging(loggingConfig, std::move(logSink));
    } else {
        tempo_utils::init_logging(loggingConfig);
    }

    // if pid file is specified, then write the pid file
    if (!agentConfig.pidFile.empty()) {
        auto pidString = absl::StrCat(getpid());
        tempo_utils::FileWriter pidWriter(agentConfig.pidFile, pidString, tempo_utils::FileWriterMode::CREATE_OR_OVERWRITE);
        TU_RETURN_IF_NOT_OK (pidWriter.getStatus());
    }

    // initialize uv loop
    uv_loop_t loop;
    uv_loop_init(&loop);

    // // construct and initialize the supervisor
    // MachineSupervisor supervisor(&loop, agentConfig.runDirectory, agentConfig.idleTimeout, agentConfig.registrationTimeout);
    // TU_RETURN_IF_NOT_OK (supervisor.initialize());

    // construct the agent service
    AgentService service(agentConfig, &loop);

    // construct the server and start it up
    grpc::ServerBuilder builder;
    std::shared_ptr<grpc::ServerCredentials> credentials;
    TU_ASSIGN_OR_RETURN (credentials, make_ssl_server_credentials(agentConfig));
    int selectedPort = 0;
    builder.AddListeningPort(agentConfig.listenLocation.toGrpcTarget(), credentials, &selectedPort);
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    // if port was autoselected then update the location
    chord_common::TransportLocation listenLocation;
    if (selectedPort > 0) {
        switch (agentConfig.listenLocation.getType()) {
            case chord_common::TransportType::Unix:
                listenLocation = agentConfig.listenLocation;
                break;
            case chord_common::TransportType::Tcp4:
                listenLocation = chord_common::TransportLocation::forTcp4(
                    agentConfig.listenLocation.getServerName(),
                    agentConfig.listenLocation.getTcp4Address(),
                    static_cast<tu_uint16>(selectedPort));
                break;
            default:
                TU_UNREACHABLE();
        }
    }

    // set the listen target on the service
    TU_RETURN_IF_NOT_OK (service.initialize(listenLocation));

    // if endpoint file is specified, then write the endpoint file
    if (!agentConfig.endpointFile.empty()) {
        tempo_utils::FileWriter endpointWriter(
            agentConfig.endpointFile, listenLocation.toString(), tempo_utils::FileWriterMode::CREATE_OR_OVERWRITE);
        TU_RETURN_IF_NOT_OK (endpointWriter.getStatus());
    }

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

    TU_LOG_V << "shutting down";
    service.shutdown();

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