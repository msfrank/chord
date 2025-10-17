/* SPDX-License-Identifier: AGPL-3.0-or-later */

#include <chord_tooling/chord_config.h>
#include <chord_tooling/tooling_result.h>
#include <tempo_config/parse_config.h>
#include <tempo_config/program_config.h>
#include <tempo_config/workspace_config.h>
#include <tempo_utils/user_home.h>

chord_tooling::ChordConfig::ChordConfig(
    const std::filesystem::path &distributionRoot,
    const std::filesystem::path &userRoot,
    const std::filesystem::path &workspaceConfigFile,
    const tempo_config::ConfigMap &chordMap,
    const tempo_config::ConfigMap &vendorMap)
    : m_distributionRoot(distributionRoot),
      m_userRoot(userRoot),
      m_workspaceConfigFile(workspaceConfigFile),
      m_chordMap(chordMap),
      m_vendorMap(vendorMap)
{
}

static tempo_utils::Result<tempo_config::ConfigMap>
load_env_override_config()
{
    const auto *value = std::getenv(chord_tooling::kEnvOverrideConfigName);
    if (value == nullptr)
        return tempo_config::ConfigMap();
    tempo_config::ConfigNode overrideNode;
    TU_ASSIGN_OR_RETURN (overrideNode, tempo_config::read_config_string(value));
    TU_LOG_V << "parsed env override config: " << overrideNode.toString();

    if (overrideNode.getNodeType() != tempo_config::ConfigNodeType::kMap)
        return tempo_config::ConfigStatus::forCondition(tempo_config::ConfigCondition::kWrongType,
            "invalid type for environment variable {}; expected a map", chord_tooling::kEnvOverrideConfigName);
    return overrideNode.toMap();
}

static tempo_utils::Result<tempo_config::ConfigMap>
load_env_override_vendor_config()
{
    const auto *value = std::getenv(chord_tooling::kEnvOverrideVendorConfigName);
    if (value == nullptr)
        return tempo_config::ConfigMap();
    tempo_config::ConfigNode overrideNode;
    TU_ASSIGN_OR_RETURN (overrideNode, tempo_config::read_config_string(value));
    TU_LOG_V << "parsed env override vendor config: " << overrideNode.toString();

    if (overrideNode.getNodeType() != tempo_config::ConfigNodeType::kMap)
        return tempo_config::ConfigStatus::forCondition(tempo_config::ConfigCondition::kWrongType,
            "invalid type for environment variable {}; expected a map", chord_tooling::kEnvOverrideVendorConfigName);
    return overrideNode.toMap();
}

static tempo_utils::Status
configure_program_config_options(
    const std::filesystem::path &distributionRoot,
    const std::filesystem::path &userRoot,
    tempo_config::ProgramConfigOptions &programConfigOptions)
{
    // load override config if present
    tempo_config::ConfigMap overrideConfig;
    TU_ASSIGN_OR_RETURN (overrideConfig, load_env_override_config());

    // load override vendor config if present
    tempo_config::ConfigMap overrideVendorConfig;
    TU_ASSIGN_OR_RETURN (overrideVendorConfig, load_env_override_vendor_config());

    programConfigOptions.toolLocator = {"chord"};
    programConfigOptions.overrideProgramConfigMap = overrideConfig;
    programConfigOptions.overrideVendorConfigMap = overrideVendorConfig;

    // set the distribution paths
    programConfigOptions.distConfigDirectoryPath = distributionRoot / CONFIG_DIR_PREFIX;
    programConfigOptions.distVendorConfigDirectoryPath = distributionRoot / VENDOR_CONFIG_DIR_PREFIX;

    // set the user paths
    if (!userRoot.empty()) {
        programConfigOptions.userConfigDirectoryPath = userRoot / tempo_config::kDefaultConfigDirectoryName;
        programConfigOptions.userVendorConfigDirectoryPath = userRoot / tempo_config::kDefaultVendorConfigDirectoryName;
    }

    return {};
}

static tempo_utils::Status
configure_workspace_config_options(
    const std::filesystem::path &distributionRoot,
    const std::filesystem::path &userRoot,
    tempo_config::WorkspaceConfigOptions &workspaceConfigOptions)
{
    // load override config if present
    tempo_config::ConfigMap overrideConfig;
    TU_ASSIGN_OR_RETURN (overrideConfig, load_env_override_config());

    // load override vendor config if present
    tempo_config::ConfigMap overrideVendorConfig;
    TU_ASSIGN_OR_RETURN (overrideVendorConfig, load_env_override_vendor_config());

    workspaceConfigOptions.toolLocator = {"chord"};
    workspaceConfigOptions.overrideWorkspaceConfigMap = overrideConfig;
    workspaceConfigOptions.overrideVendorConfigMap = overrideVendorConfig;

    // set the distribution paths
    workspaceConfigOptions.distConfigDirectoryPath = distributionRoot / CONFIG_DIR_PREFIX;
    workspaceConfigOptions.distVendorConfigDirectoryPath = distributionRoot / VENDOR_CONFIG_DIR_PREFIX;

    // set the user paths
    if (!userRoot.empty()) {
        workspaceConfigOptions.userConfigDirectoryPath = userRoot / tempo_config::kDefaultConfigDirectoryName;
        workspaceConfigOptions.userVendorConfigDirectoryPath = userRoot / tempo_config::kDefaultVendorConfigDirectoryName;
    }

    return {};
}

/**
 * Load chord config from the system distribution.
 *
 * @param distributionRootOverride Override the auto-detected distribution root.
 * @return
 */
tempo_utils::Result<std::shared_ptr<chord_tooling::ChordConfig>>
chord_tooling::ChordConfig::forSystem(const std::filesystem::path &distributionRootOverride)
{
    std::filesystem::path distributionRoot = !distributionRootOverride.empty()?
        distributionRootOverride : std::filesystem::path(DISTRIBUTION_ROOT);
    if (!std::filesystem::exists(distributionRoot))
        return ToolingStatus::forCondition(ToolingCondition::kToolingInvariant,
            "distribution root '{}' does not exist", distributionRoot.string());

    tempo_config::ProgramConfigOptions programConfigOptions;
    TU_RETURN_IF_NOT_OK (configure_program_config_options(
        distributionRoot, {}, programConfigOptions));

    std::shared_ptr<tempo_config::ProgramConfig> programConfig;
    TU_ASSIGN_OR_RETURN (programConfig, tempo_config::ProgramConfig::load(programConfigOptions));

    auto applicationMap = programConfig->getToolConfig();
    auto vendorMap = programConfig->getVendorConfig();

    auto chordConfig = std::shared_ptr<ChordConfig>(new ChordConfig(
        distributionRoot, {}, {}, applicationMap, vendorMap));
    TU_RETURN_IF_NOT_OK (chordConfig->configure());

    return chordConfig;
}

/**
 * Load chord config for the user.
 *
 * @param userHomeOverride Override the auto-detected user home directory.
 * @param distributionRootOverride Override the auto-detected distribution root.
 * @return
 */
tempo_utils::Result<std::shared_ptr<chord_tooling::ChordConfig>>
chord_tooling::ChordConfig::forUser(
    const std::filesystem::path &userHomeOverride,
    const std::filesystem::path &distributionRootOverride)
{
    std::filesystem::path distributionRoot = !distributionRootOverride.empty()?
        distributionRootOverride : std::filesystem::path(DISTRIBUTION_ROOT);
    if (!std::filesystem::exists(distributionRoot))
        return ToolingStatus::forCondition(ToolingCondition::kToolingInvariant,
            "distribution root '{}' does not exist", distributionRoot.string());

    std::filesystem::path userHome = !userHomeOverride.empty()?
        userHomeOverride : tempo_utils::get_user_home_directory();
    auto userRoot = userHome / kDefaultUserDirectoryName;
    if (!std::filesystem::exists(userRoot)) {
        userRoot.clear();
    }

    tempo_config::ProgramConfigOptions programConfigOptions;
    TU_RETURN_IF_NOT_OK (configure_program_config_options(
        distributionRoot, userRoot, programConfigOptions));

    std::shared_ptr<tempo_config::ProgramConfig> programConfig;
    TU_ASSIGN_OR_RETURN (programConfig, tempo_config::ProgramConfig::load(programConfigOptions));

    auto applicationMap = programConfig->getToolConfig();
    auto vendorMap = programConfig->getVendorConfig();

    auto chordConfig = std::shared_ptr<ChordConfig>(new ChordConfig(
        distributionRoot, userRoot, {}, applicationMap, vendorMap));
    TU_RETURN_IF_NOT_OK (chordConfig->configure());

    return chordConfig;
}

/**
 * Load chord config from the specified workspace.config file.
 *
 * @param workspaceConfigFile The path to the workspace.config file.
 * @param userHomeOverride Override the auto-detected user home directory.
 * @param distributionRootOverride Override the auto-detected distribution root.
 * @return
 */
tempo_utils::Result<std::shared_ptr<chord_tooling::ChordConfig>>
chord_tooling::ChordConfig::forWorkspace(
    const std::filesystem::path &workspaceConfigFile,
    const std::filesystem::path &userHomeOverride,
    const std::filesystem::path &distributionRootOverride)
{
    std::filesystem::path distributionRoot = !distributionRootOverride.empty()?
        distributionRootOverride : std::filesystem::path(DISTRIBUTION_ROOT);
    if (!std::filesystem::exists(distributionRoot))
        return ToolingStatus::forCondition(ToolingCondition::kToolingInvariant,
            "distribution root '{}' does not exist", distributionRoot.string());

    std::filesystem::path userHome = !userHomeOverride.empty()?
        userHomeOverride : tempo_utils::get_user_home_directory();
    auto userRoot = userHome / kDefaultUserDirectoryName;
    if (!std::filesystem::exists(userRoot)) {
        userRoot.clear();
    }

    tempo_config::WorkspaceConfigOptions workspaceConfigOptions;
    TU_RETURN_IF_NOT_OK (configure_workspace_config_options(
        distributionRoot, userRoot, workspaceConfigOptions));

    std::shared_ptr<tempo_config::WorkspaceConfig> workspaceConfig;
    TU_ASSIGN_OR_RETURN (workspaceConfig, tempo_config::WorkspaceConfig::load(
        workspaceConfigFile, workspaceConfigOptions));

    auto applicationMap = workspaceConfig->getToolConfig();
    auto vendorMap = workspaceConfig->getVendorConfig();

    auto chordConfig = std::shared_ptr<ChordConfig>(new ChordConfig(
        distributionRoot, userRoot, workspaceConfigFile, applicationMap, vendorMap));
    TU_RETURN_IF_NOT_OK (chordConfig->configure());

    return chordConfig;
}

tempo_utils::Status
chord_tooling::ChordConfig::configure()
{
    if (m_chordMap.getNodeType() != tempo_config::ConfigNodeType::kMap)
        return ToolingStatus::forCondition(ToolingCondition::kToolingInvariant,
            "invalid 'chord' config node; expected a map");

    auto agentsMap = m_chordMap.mapAt("agents").toMap();
    if (agentsMap.getNodeType() == tempo_config::ConfigNodeType::kMap) {
        m_agentStore = std::make_shared<AgentStore>(agentsMap);
        TU_RETURN_IF_NOT_OK (m_agentStore->configure());
    }

    auto securityMap = m_chordMap.mapAt("security").toMap();
    m_securityConfig = std::make_shared<SecurityConfig>(securityMap);
    TU_RETURN_IF_NOT_OK (m_securityConfig->configure());

    return {};
}

std::filesystem::path
chord_tooling::ChordConfig::getDistributionRoot() const
{
    return m_distributionRoot;
}

std::filesystem::path
chord_tooling::ChordConfig::getUserRoot() const
{
    return m_userRoot;
}

std::filesystem::path
chord_tooling::ChordConfig::getWorkspaceRoot() const
{
    return m_workspaceConfigFile.parent_path();
}

std::filesystem::path
chord_tooling::ChordConfig::getWorkspaceConfigFile() const
{
    return m_workspaceConfigFile;
}

std::shared_ptr<chord_tooling::AgentStore>
chord_tooling::ChordConfig::getAgentStore() const
{
    return m_agentStore;
}

std::shared_ptr<chord_tooling::SecurityConfig>
chord_tooling::ChordConfig::getSecurityConfig() const
{
    return m_securityConfig;
}
