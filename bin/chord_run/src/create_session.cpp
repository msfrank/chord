
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