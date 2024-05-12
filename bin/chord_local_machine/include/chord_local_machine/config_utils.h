#ifndef CHORD_LOCAL_MACHINE_CONFIG_UTILS_H
#define CHORD_LOCAL_MACHINE_CONFIG_UTILS_H

#include <filesystem>
#include <string>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include <tempo_utils/status.h>
#include <tempo_utils/url.h>
#include <lyric_common/assembly_location.h>

struct ChordLocalMachineConfig {
    std::filesystem::path runDirectory;
    std::filesystem::path installDirectory;
    std::vector<std::filesystem::path> packageDirectories;
    absl::flat_hash_set<tempo_utils::Url> expectedPorts;
    bool startSuspended;
    tempo_utils::Url supervisorUrl;
    std::string supervisorNameOverride;
    tempo_utils::Url machineUrl;
    std::string machineNameOverride;
    std::filesystem::path pemRootCABundleFile;
    std::filesystem::path logFile;
    lyric_common::AssemblyLocation mainLocation;
    std::string binderEndpoint;
    std::string binderOrganization;
    std::string binderOrganizationalUnit;
    std::string binderCsrFilenameStem;
};

tempo_utils::Status configure(ChordLocalMachineConfig &chordLocalMachineConfig, int argc, const char *argv[]);

#endif // CHORD_LOCAL_MACHINE_CONFIG_UTILS_H
