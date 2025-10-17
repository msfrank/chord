#ifndef CHORD_RUN_RUN_PACKAGE_COMMAND_H
#define CHORD_RUN_RUN_PACKAGE_COMMAND_H

#include <tempo_utils/status.h>
#include <zuri_packager/package_specifier.h>

namespace chord_run {
    tempo_utils::Status run_package_command(
        const tempo_utils::Url &agentEndpoint,
        const std::string &agentServerName,
        const std::filesystem::path &pemRootCABundleFile,
        const zuri_packager::PackageSpecifier &packageSpecifier,
        const std::vector<std::string> &mainArgs);
}

#endif // CHORD_RUN_RUN_PACKAGE_COMMAND_H