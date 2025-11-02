#ifndef CHORD_MACHINE_CONFIG_UTILS_H
#define CHORD_MACHINE_CONFIG_UTILS_H

#include <filesystem>
#include <string>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include <chord_common/common_conversions.h>
#include <tempo_utils/status.h>
#include <tempo_utils/url.h>
#include <zuri_packager/package_reader.h>

namespace chord_machine {

    struct ChordLocalMachineConfig {
        std::string machineName;
        std::filesystem::path runDirectory;
        chord_common::TransportLocation supervisorEndpoint;
        std::vector<std::filesystem::path> packageCacheDirectories;
        absl::flat_hash_set<tempo_utils::Url> expectedPorts;
        bool startSuspended;
        std::filesystem::path pemRootCABundleFile;
        std::filesystem::path logFile;
        zuri_packager::PackageSpecifier mainPackage;
        std::vector<std::string> mainArguments;
        chord_common::TransportLocation binderEndpoint;
        std::string binderOrganization;
        std::string binderOrganizationalUnit;
        std::string binderCsrFilenameStem;
    };

    tempo_utils::Status configure(
        ChordLocalMachineConfig &chordLocalMachineConfig,
        int argc,
        const char *argv[]);
}

#endif // CHORD_MACHINE_CONFIG_UTILS_H
