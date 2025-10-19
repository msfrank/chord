
#include <chord_run/create_session.h>
#include <chord_run/run_result.h>
#include <tempo_security/ecc_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_utils/directory_maker.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/file_writer.h>
#include <tempo_utils/tempdir_maker.h>
#include <tempo_utils/uuid.h>

tempo_utils::Result<std::shared_ptr<chord_sandbox::ChordIsolate>>
chord_run::create_session(
    const std::string &sessionIsolate,
    const std::string &sessionNameOverride,
    std::shared_ptr<chord_tooling::ChordConfig> chordConfig,
    const std::filesystem::path &pemRootCABundleFileOverride)
{
    TU_ASSERT (!sessionIsolate.empty());

    auto agentStore = chordConfig->getAgentStore();
    auto agentEntry = agentStore->getAgent(sessionIsolate);
    if (agentEntry == nullptr)
        return RunStatus::forCondition(RunCondition::kRunInvariant,
            "isolate '{}' does not exist", sessionIsolate);

    auto securityConfig = chordConfig->getSecurityConfig();

    std::filesystem::path pemRootCABundleFile;
    if (!pemRootCABundleFileOverride.empty()) {
        pemRootCABundleFile = pemRootCABundleFileOverride;
    } else {
        pemRootCABundleFile = securityConfig->pemRootCABundleFile;
    }

    if (!std::filesystem::is_regular_file(pemRootCABundleFile))
        return RunStatus::forCondition(RunCondition::kRunInvariant,
            "missing root CA bundle file {}", pemRootCABundleFile.c_str());

    std::filesystem::path runDirectory("/var/chord");
    if (!std::filesystem::is_directory(runDirectory))
        return RunStatus::forCondition(RunCondition::kRunInvariant,
            "missing run directory {}", runDirectory.c_str());

    auto uid = getuid();
    auto userSessionsDirectory = runDirectory / absl::StrCat("sessions-", uid);
    if (!std::filesystem::exists(userSessionsDirectory)) {
        tempo_utils::DirectoryMaker dir(userSessionsDirectory, std::filesystem::perms::owner_all);
        TU_RETURN_IF_NOT_OK (dir.getStatus());
    }

    std::string sessionName;
    if (!sessionNameOverride.empty()) {
        sessionName = sessionNameOverride;
    } else {
        sessionName = tempo_utils::generate_name(absl::StrCat(sessionIsolate, ".XXXXXXXX"));
    }
    tempo_utils::DirectoryMaker dir(userSessionsDirectory, sessionName, std::filesystem::perms::owner_all);
    TU_RETURN_IF_NOT_OK (dir.getStatus());
    auto sessionDirectory = dir.getAbsolutePath();
}

tempo_utils::Result<std::shared_ptr<chord_sandbox::ChordIsolate>>
create_isolate(
    const std::string &agentName,
    const std::filesystem::path &sessionDirectory,
    const std::filesystem::path &pemRootCABundleFile,
    std::shared_ptr<const chord_tooling::SecurityConfig> securityConfig,
    std::shared_ptr<const chord_tooling::AgentEntry> agentEntry)
{
    // generate session id
    auto sessionId = tempo_utils::UUID::randomUUID();
    auto sessionString = sessionId.toString();
    auto sessionCommonName = absl::StrCat(sessionString, ".session.chord.alt");

    // write the sid file
    tempo_utils::FileWriter sidWriter(
        sessionDirectory / "sid", sessionCommonName, tempo_utils::FileWriterMode::CREATE_ONLY);
    TU_RETURN_IF_NOT_OK (sidWriter.getStatus());

    std::error_code ec;

    // copy the root CA bundle file
    std::filesystem::copy(pemRootCABundleFile, sessionDirectory, ec);
    if (!ec)
        return chord_run::RunStatus::forCondition(chord_run::RunCondition::kRunInvariant,
            "failed to copy root CA bundle file: {}", ec.message());

    // generate the agent keypair
    std::string organization = "chord.alt";
    std::string organizationalUnit = "session";
    int serialNumber = 1;
    absl::Duration validityPeriod = absl::Hours(4);

    tempo_security::ECCPrivateKeyGenerator keygen(NID_X9_62_prime256v1);

    tempo_security::CertificateKeyPair signingKeyPair;
    TU_ASSIGN_OR_RETURN (signingKeyPair, securityConfig->getSigningKeypair());

    tempo_security::CertificateKeyPair agentKeyPair;
    TU_ASSIGN_OR_RAISE (agentKeyPair, tempo_security::generate_key_pair(
        signingKeyPair, keygen,
        organization, organizationalUnit, sessionCommonName,
        serialNumber, absl::ToChronoSeconds(validityPeriod),
        sessionDirectory, "agent"));

    return chord_sandbox::ChordIsolate::spawn(agentName,)
}