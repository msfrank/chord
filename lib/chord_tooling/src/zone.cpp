
#include <chord_tooling/tooling_result.h>
#include <chord_tooling/zone.h>
#include <tempo_config/config_utils.h>
#include <zuri_tooling/environment.h>

chord_tooling::Zone::Zone()
{
}

chord_tooling::Zone::Zone(
    const std::filesystem::path &zoneConfigFile,
    const std::filesystem::path &zoneDirectory,
    const std::filesystem::path &configDirectory,
    const std::filesystem::path &environmentDirectory)
    : m_priv(std::make_shared<Priv>(
        zoneConfigFile,
        zoneDirectory,
        configDirectory,
        environmentDirectory))
{
    TU_ASSERT (!m_priv->zoneConfigFile.empty());
    TU_ASSERT (!m_priv->zoneDirectory.empty());
    TU_ASSERT (!m_priv->configDirectory.empty());
    TU_ASSERT (!m_priv->environmentDirectory.empty());
}

chord_tooling::Zone::Zone(const Zone &other)
    : m_priv(other.m_priv)
{
}

bool
chord_tooling::Zone::isValid() const
{
    return m_priv != nullptr;
}

std::filesystem::path
chord_tooling::Zone::getZoneConfigFile() const
{
    if (m_priv)
        return m_priv->zoneConfigFile;
    return {};
}

std::filesystem::path
chord_tooling::Zone::getZoneDirectory() const
{
    if (m_priv)
        return m_priv->zoneDirectory;
    return {};
}

std::filesystem::path
chord_tooling::Zone::getConfigDirectory() const
{
    if (m_priv)
        return m_priv->configDirectory;
    return {};
}

std::filesystem::path
chord_tooling::Zone::getEnvironmentDirectory() const
{
    if (m_priv)
        return m_priv->environmentDirectory;
    return {};
}

tempo_utils::Result<chord_tooling::Zone>
chord_tooling::Zone::openOrCreate(
    const std::filesystem::path &zoneDirectory,
    const ZoneOpenOrCreateOptions &options)
{
    if (std::filesystem::exists(zoneDirectory)) {
        if (options.exclusive)
            return ToolingStatus::forCondition(ToolingCondition::kToolingInvariant,
                "chord zone directory {} already exists", zoneDirectory.string());
        return open(zoneDirectory);
    }

    std::error_code ec;

    // create the zone root
    std::filesystem::create_directory(zoneDirectory, ec);
    if (ec)
        return ToolingStatus::forCondition(ToolingCondition::kToolingInvariant,
            "failed to create chord zone directory {}", zoneDirectory.string());

    // create the config directory
    auto configDirectory = zoneDirectory / "config";
    std::filesystem::create_directory(configDirectory, ec);
    if (ec)
        return ToolingStatus::forCondition(ToolingCondition::kToolingInvariant,
            "failed to create zone config directory {}", configDirectory.string());

    // create the env directory
    zuri_tooling::EnvironmentOpenOrCreateOptions environmentOpenOrCreateOptions;
    environmentOpenOrCreateOptions.distribution = options.distribution;
    environmentOpenOrCreateOptions.extraLibDirs = options.extraLibDirs;
    environmentOpenOrCreateOptions.exclusive = true;
    auto environmentDirectory = zoneDirectory / "env";
    zuri_tooling::Environment environment;
    TU_ASSIGN_OR_RETURN (environment, zuri_tooling::Environment::openOrCreate(
        environmentDirectory, environmentOpenOrCreateOptions));

    // write the zone.config
    auto zoneConfigFile = zoneDirectory / kZoneConfigName;
    TU_RETURN_IF_NOT_OK (tempo_config::write_config_file(options.zoneMap, zoneConfigFile));

    return Zone(zoneConfigFile, zoneDirectory, configDirectory, environmentDirectory);
}

tempo_utils::Result<chord_tooling::Zone>
chord_tooling::Zone::open(const std::filesystem::path &zoneDirectoryOrConfigFile)
{
    std::filesystem::path zoneConfigFile;
    std::filesystem::path zoneDirectory;

    if (std::filesystem::is_regular_file(zoneDirectoryOrConfigFile)) {
        zoneConfigFile = zoneDirectoryOrConfigFile;
        zoneDirectory = zoneConfigFile.parent_path();
    } else if (std::filesystem::is_directory(zoneDirectoryOrConfigFile)) {
        zoneDirectory = zoneDirectoryOrConfigFile;
        zoneConfigFile = zoneDirectory / kZoneConfigName;
    } else {
        return ToolingStatus::forCondition(ToolingCondition::kToolingInvariant,
            "chord zone not found at {}", zoneDirectoryOrConfigFile.string());
    }

    if (!std::filesystem::exists(zoneConfigFile))
        return ToolingStatus::forCondition(ToolingCondition::kToolingInvariant,
            "missing chord zone config file {}", zoneConfigFile.string());

    auto configDirectory = zoneDirectory / "config";
    auto environmentDirectory = zoneDirectory / "env";

    // verify environment directory exists
    if (!std::filesystem::exists(environmentDirectory))
        return ToolingStatus::forCondition(ToolingCondition::kToolingInvariant,
            "missing zone environment directory {}", environmentDirectory.string());

    return Zone(zoneConfigFile, zoneDirectory, configDirectory, environmentDirectory);
}

tempo_utils::Result<chord_tooling::Zone>
chord_tooling::Zone::find(const std::filesystem::path &searchStart)
{
    // the initial search path must exist and be a directory
    if (!std::filesystem::exists(searchStart))
        return Zone{};

    // if searchStart is not a directory, then cd to the parent directory
    auto currentDirectory = searchStart;
    if (!std::filesystem::is_directory(currentDirectory)) {
        currentDirectory = currentDirectory.parent_path();
    }

    // check each parent directory for a file called "zone.config". if the file is found then we have
    // determined the zone root. otherwise if no file is found then zone detection failed.
    while (currentDirectory != currentDirectory.root_path()) {
        auto file = currentDirectory / kZoneConfigName;
        if (std::filesystem::exists(file))
            return open(currentDirectory);
        currentDirectory = currentDirectory.parent_path();
    }

    // we were unable to find the zone
    return Zone{};
}
