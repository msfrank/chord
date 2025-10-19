#ifndef CHORD_RUN_CREATE_SESSION_H
#define CHORD_RUN_CREATE_SESSION_H

#include <chord_sandbox/chord_isolate.h>

namespace chord_run {

    tempo_utils::Result<std::shared_ptr<chord_sandbox::ChordIsolate>> create_session(
        const std::string &sessionIsolate,
        const std::string &sessionNameOverride,
        std::shared_ptr<chord_tooling::ChordConfig> chordConfig,
        const std::filesystem::path &pemRootCABundleFileOverride);
}

#endif // CHORD_RUN_CREATE_SESSION_H