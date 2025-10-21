/* SPDX-License-Identifier: AGPL-3.0-or-later */

#ifndef CHORD_TOOLING_CHORD_CONFIG_H
#define CHORD_TOOLING_CHORD_CONFIG_H

#include <tempo_config/config_types.h>
#include <tempo_utils/result.h>
#include <tempo_utils/url.h>

#include "agent_store.h"
#include "security_config.h"
#include "signer_store.h"

namespace chord_tooling {

    constexpr const char * const kEnvOverrideConfigName = "CHORD_OVERRIDE_CONFIG";
    constexpr const char * const kEnvOverrideVendorConfigName = "CHORD_OVERRIDE_VENDOR_CONFIG";
    constexpr const char * const kDefaultUserDirectoryName = ".chord";

    /**
     *
     */
    class ChordConfig {

    public:
        static tempo_utils::Result<const std::shared_ptr<ChordConfig>> forSystem(
            const std::filesystem::path &distributionRootOverride = {});
        static tempo_utils::Result<const std::shared_ptr<ChordConfig>> forUser(
            const std::filesystem::path &userHomeOverride = {},
            const std::filesystem::path &distributionRootOverride = {});
        static tempo_utils::Result<const std::shared_ptr<ChordConfig>> forWorkspace(
            const std::filesystem::path &workspaceConfigFile,
            const std::filesystem::path &userHomeOverride = {},
            const std::filesystem::path &distributionRootOverride = {});

        std::filesystem::path getDistributionRoot() const;
        std::filesystem::path getUserRoot() const;
        std::filesystem::path getWorkspaceRoot() const;
        std::filesystem::path getWorkspaceConfigFile() const;

        std::shared_ptr<const SignerStore> getSignerStore() const;
        std::shared_ptr<const AgentStore> getAgentStore() const;
        std::shared_ptr<const SecurityConfig> getSecurityConfig() const;

        std::filesystem::path getRunDirectory() const;
        std::string getDefaultSignerName() const;
        std::string getDefaultAgentName() const;

    private:
        std::filesystem::path m_distributionRoot;
        std::filesystem::path m_userRoot;
        std::filesystem::path m_workspaceConfigFile;
        tempo_config::ConfigMap m_chordMap;
        tempo_config::ConfigMap m_vendorMap;

        std::shared_ptr<const SignerStore> m_signerStore;
        std::shared_ptr<const AgentStore> m_agentStore;
        std::shared_ptr<const SecurityConfig> m_securityConfig;

        std::filesystem::path m_runDirectory;
        std::string m_defaultSignerName;
        std::string m_defaultAgentName;

        ChordConfig(
            const std::filesystem::path &distributionRoot,
            const std::filesystem::path &userRoot,
            const std::filesystem::path &workspaceConfigFile,
            const tempo_config::ConfigMap &chordMap,
            const tempo_config::ConfigMap &vendorMap);

        tempo_utils::Status configure();
    };
}

#endif // CHORD_TOOLING_CHORD_CONFIG_H
