#ifndef CHORD_MACHINE_CONFIG_UTILS_H
#define CHORD_MACHINE_CONFIG_UTILS_H

#include <filesystem>
#include <string>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include <tempo_utils/status.h>
#include <tempo_utils/url.h>
#include <lyric_common/module_location.h>

namespace chord_machine {

    struct ChordLocalMachineConfig {
        std::filesystem::path runDirectory;
        std::vector<std::filesystem::path> packageCacheDirectories;
        absl::flat_hash_set<tempo_utils::Url> expectedPorts;
        bool startSuspended;
        tempo_utils::Url supervisorUrl;
        std::string supervisorNameOverride;
        tempo_utils::Url machineUrl;
        std::string machineNameOverride;
        std::filesystem::path pemRootCABundleFile;
        std::filesystem::path logFile;
        tempo_utils::Url mainLocation;
        std::string binderEndpoint;
        std::string binderOrganization;
        std::string binderOrganizationalUnit;
        std::string binderCsrFilenameStem;
    };

    tempo_utils::Status configure(ChordLocalMachineConfig &chordLocalMachineConfig, int argc, const char *argv[]);
}

#endif // CHORD_MACHINE_CONFIG_UTILS_H
