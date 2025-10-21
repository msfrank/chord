
#include <numbers>

#include <absl/strings/ascii.h>

#include <chord_sandbox/internal/session_utils.h>
#include <tempo_security/ecc_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_utils/file_reader.h>
#include <tempo_utils/file_writer.h>
#include <tempo_utils/process_builder.h>
#include <tempo_utils/uuid.h>

tempo_utils::Result<chord_sandbox::internal::PrepareSessionResult>
chord_sandbox::internal::prepare_session(
    const std::filesystem::path &sessionDirectory,
    const std::filesystem::path &pemRootCABundleFile,
    std::shared_ptr<chord_common::AbstractCertificateSigner> certificateSigner)
{
    PrepareSessionResult prepareSessionResult;

    // generate session id
    auto sessionId = tempo_utils::UUID::randomUUID().toString();
    prepareSessionResult.sessionId = sessionId;
    auto commonName = absl::StrCat(prepareSessionResult.sessionId, ".session.chord.alt");
    prepareSessionResult.commonName = commonName;
    auto sessionUrl =tempo_utils::Url::fromAbsolute("chord+session", commonName);

    // write the sid file
    tempo_utils::FileWriter sidWriter(
        sessionDirectory / "sid", commonName, tempo_utils::FileWriterMode::CREATE_ONLY);
    TU_RETURN_IF_NOT_OK (sidWriter.getStatus());

    std::error_code ec;

    // copy the root CA bundle file
    std::filesystem::copy(pemRootCABundleFile, sessionDirectory, ec);
    if (ec)
        return chord_sandbox::SandboxStatus::forCondition(chord_sandbox::SandboxCondition::kSandboxInvariant,
            "failed to copy root CA bundle file: {}", ec.message());

    // generate the agent keypair
    std::string organization = "chord.alt";
    std::string organizationalUnit = "session";
    absl::Duration requestedValidityPeriod = absl::Hours(24);

    tempo_security::ECCPrivateKeyGenerator keygen(tempo_security::ECCurveId::Prime256v1);

    // write the session key and signing request files
    tempo_security::CSRKeyPair csr;
    TU_ASSIGN_OR_RETURN (csr, tempo_security::generate_csr_key_pair(
        keygen, organization, organizationalUnit, commonName, sessionDirectory, "agent"));
    prepareSessionResult.pemPrivateKeyFile = csr.getPemPrivateKeyFile();

    tempo_utils::FileReader pemRequestReader(csr.getPemRequestFile());
    TU_RETURN_IF_NOT_OK (pemRequestReader.getStatus());
    auto bytes = pemRequestReader.getBytes();
    std::string_view pemRequestBytes((const char *) bytes->getData(), bytes->getSize());

    std::string pemCertificateBytes;
    TU_ASSIGN_OR_RETURN (pemCertificateBytes, certificateSigner->signSession(
        sessionUrl, pemRequestBytes, requestedValidityPeriod));

    // write the session certificate file
    tempo_utils::FileWriter pemCertificateWriter(
        sessionDirectory / "agent.crt", pemCertificateBytes, tempo_utils::FileWriterMode::CREATE_ONLY);
    TU_RETURN_IF_NOT_OK (pemCertificateWriter.getStatus());
    prepareSessionResult.pemCertificateFile = pemCertificateWriter.getAbsolutePath();

    return prepareSessionResult;
}

tempo_utils::Result<chord_sandbox::internal::SpawnSessionResult>
chord_sandbox::internal::spawn_session(
    const std::filesystem::path &sessionDirectory,
    const std::filesystem::path &agentPath,
    const std::filesystem::path &pemRootCABundleFile,
    const std::filesystem::path &pemAgentCertificateFile,
    const std::filesystem::path &pemAgentPrivateKeyFile,
    absl::Duration idleTimeout,
    absl::Duration registrationTimeout)
{
    SpawnSessionResult spawnSessionResult;

    std::filesystem::path exepath;
    if (!agentPath.empty()) {
        exepath = agentPath;
    } else {
        exepath = CHORD_AGENT_EXECUTABLE;
    }

    tempo_utils::ProcessBuilder builder(exepath);

    builder.appendArg("--temporary-session");
    builder.appendArg("--listen-transport", "Unix");
    builder.appendArg("--background");
    builder.appendArg("--certificate", pemAgentCertificateFile.string());
    builder.appendArg("--private-key", pemAgentPrivateKeyFile.string());
    builder.appendArg("--ca-bundle", pemRootCABundleFile.string());

    auto sessionName = sessionDirectory.filename().string();
    builder.appendArg("--session-name", sessionName);

    auto endpointFile = sessionDirectory / "endpoint";
    builder.appendArg("--endpoint-file", endpointFile.c_str());

    auto logFile = sessionDirectory / "log";
    builder.appendArg("--log-file", logFile.c_str());

    auto pidFile = sessionDirectory / "pid";
    builder.appendArg("--pid-file", pidFile.c_str());

    auto idleTimeoutInSeconds = absl::ToInt64Seconds(idleTimeout);
    if (idleTimeoutInSeconds > 0) {
        builder.appendArg("--idle-timeout", absl::StrCat(idleTimeoutInSeconds));
    }

    auto registrationTimeoutInSeconds = absl::ToInt64Seconds(registrationTimeout);
    if (registrationTimeoutInSeconds > 0) {
        builder.appendArg("--registration-timeout", absl::StrCat(registrationTimeoutInSeconds));
    }

    auto invoker = builder.toInvoker();
    TU_LOG_INFO << "invoking " << invoker.toString();

    spawnSessionResult.process = std::make_shared<tempo_utils::ProcessRunner>(invoker, sessionDirectory);
    TU_RETURN_IF_NOT_OK (spawnSessionResult.process->getStatus());

    TU_ASSIGN_OR_RETURN (spawnSessionResult.endpoint, load_session_endpoint(
        sessionDirectory, absl::Seconds(15)));

    return spawnSessionResult;
}

tempo_utils::Result<chord_sandbox::internal::LoadSessionResult>
chord_sandbox::internal::load_session(const std::filesystem::path &sessionDirectory)
{
    if (!std::filesystem::is_directory(sessionDirectory))
        return SandboxStatus::forCondition(SandboxCondition::kInvalidConfiguration,
            "session not found at {}", sessionDirectory.c_str());

    LoadSessionResult loadSessionResult;

    tempo_utils::FileReader endpointReader(sessionDirectory / "endpoint");
    TU_RETURN_IF_NOT_OK (endpointReader.getStatus());
    auto endpointBytes = endpointReader.getBytes();
    auto endpoint = std::string_view((const char *) endpointBytes->getData(), endpointBytes->getSize());
    loadSessionResult.endpoint = chord_common::TransportLocation::fromString(endpoint);

    tempo_utils::FileReader sidReader(sessionDirectory / "sid");
    TU_RETURN_IF_NOT_OK (sidReader.getStatus());
    auto sidBytes = sidReader.getBytes();
    loadSessionResult.sessionId = std::string((const char *) sidBytes->getData(), sidBytes->getSize());

    tempo_utils::FileReader pidReader(sessionDirectory / "pid");
    TU_RETURN_IF_NOT_OK (pidReader.getStatus());
    auto pidBytes = pidReader.getBytes();
    loadSessionResult.processId = std::string((const char *) pidBytes->getData(), pidBytes->getSize());

    return loadSessionResult;
}

tempo_utils::Result<chord_common::TransportLocation>
chord_sandbox::internal::load_session_endpoint(const std::filesystem::path &sessionDirectory, absl::Duration timeout)
{
    auto millisRemaining = absl::ToInt64Milliseconds(timeout);
    int iteration = 1;
    tempo_utils::Status status;

    do {
        auto sleepMillis = absl::Milliseconds(iteration * 10 * std::numbers::e);
        absl::SleepFor(sleepMillis);
        millisRemaining -= ToInt64Milliseconds(sleepMillis);

        tempo_utils::FileReader endpointReader(sessionDirectory / "endpoint");
        if (endpointReader.isValid()) {
            auto endpointBytes = endpointReader.getBytes();
            auto endpoint = std::string_view((const char *) endpointBytes->getData(), endpointBytes->getSize());
            return chord_common::TransportLocation::fromString(endpoint);
        }
        status = endpointReader.getStatus();

    } while (millisRemaining > 0);

    return status;
}
