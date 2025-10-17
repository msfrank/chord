#include <iostream>

#include <chord_sandbox/chord_isolate.h>
#include <chord_test/chord_sandbox_tester.h>
#include <lyric_build/build_attrs.h>
#include <lyric_build/metadata_writer.h>
#include <lyric_common/common_types.h>
#include <lyric_test/test_result.h>
#include <tempo_config/config_builder.h>
#include <tempo_security/ecc_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_utils/directory_maker.h>
#include <tempo_utils/file_utilities.h>
#include <zuri_packager/package_writer.h>
#include <zuri_test/placeholder_loader.h>

chord_test::ChordSandboxTester::ChordSandboxTester(const SandboxTesterOptions &options)
    : m_options(options),
      m_eccKeygen(NID_X9_62_prime256v1)
{

}

tempo_utils::Status
chord_test::ChordSandboxTester::configure()
{
    if (m_runner != nullptr)
        return lyric_test::TestStatus::forCondition(lyric_test::TestCondition::kTestInvariant,
            "tester is already configured");

    auto placeholderLoader = std::make_shared<zuri_test::PlaceholderLoader>();

    // construct the test runner
    auto runner = lyric_test::TestRunner::create(
        m_options.testRootDirectory,
        m_options.useInMemoryCache,
        m_options.isTemporary,
        m_options.keepBuildOnUnexpectedResult,
        m_options.taskRegistry,
        m_options.bootstrapLoader,
        placeholderLoader,
        m_options.taskSettings);

    // configure the test runner
    TU_RETURN_IF_NOT_OK (runner->configureBaseTester());

    m_domain = absl::StrCat(tempo_utils::generate_name("XXXXXXXX"), ".test");

    TU_ASSIGN_OR_RETURN (m_caKeyPair, tempo_security::generate_self_signed_ca_key_pair(
        m_eccKeygen, "Chord sandbox tester", "Certificate authority",
        absl::StrCat("ca.", m_domain),
        1, std::chrono::minutes {60}, -1,
        runner->getTesterDirectory(), "ca"));

    // open the existing package cache if specified, otherwise create a new one
    std::shared_ptr<zuri_distributor::PackageCache> packageCache;
    if (m_options.existingPackageCache.empty()) {
        TU_ASSIGN_OR_RETURN (packageCache, zuri_distributor::PackageCache::openOrCreate(
            runner->getTesterDirectory(), "packages"));
    } else {
        TU_ASSIGN_OR_RETURN (packageCache, zuri_distributor::PackageCache::open(
            m_options.existingPackageCache));
    }

    // install local packages
    for (const auto &packagePath : m_options.localPackages) {
        TU_RETURN_IF_STATUS (packageCache->installPackage(packagePath));
    }

    // configure requirements loader
    auto packageCacheLoader = std::make_shared<zuri_distributor::PackageCacheLoader>(packageCache);
    TU_RETURN_IF_NOT_OK (placeholderLoader->resolve(packageCacheLoader));

    m_packageCacheLoader = packageCacheLoader;
    m_packageCache = std::move(packageCache);
    m_runner = std::move(runner);

    return {};
}

const lyric_test::TestRunner *
chord_test::ChordSandboxTester::getRunner() const
{
    return m_runner.get();
}

tempo_utils::Result<chord_test::SpawnMachine>
chord_test::ChordSandboxTester::spawnProgramInSandbox(const tempo_utils::Url &mainLocation)
{
    if (!m_runner->isConfigured())
        return lyric_test::TestStatus::forCondition(lyric_test::TestCondition::kTestInvariant,
            "tester is unconfigured");

    TU_CONSOLE_OUT << "";
    TU_CONSOLE_OUT << "======== SPAWN: " << mainLocation.toString() << " ========";
    TU_CONSOLE_OUT << "";

    auto agentDomain = tempo_utils::generate_name("chord-sandbox-tester-XXXXXXXX");
    auto agentServerName = absl::StrCat(agentDomain, ".test");
    auto generateAgentKeyPairResult = tempo_security::generate_key_pair(m_caKeyPair, m_eccKeygen,
        "Chord sandbox tester", "Chord agent", agentServerName,
        1, std::chrono::seconds{60},
        m_runner->getTesterDirectory(), "agent");
    if (generateAgentKeyPairResult.isStatus())
        return generateAgentKeyPairResult.getStatus();
    auto agentKeyPair = generateAgentKeyPairResult.getResult();

    // construct the sandbox
    chord_sandbox::SandboxOptions options;
    options.discoveryPolicy = chord_sandbox::AgentDiscoveryPolicy::ALWAYS_SPAWN;
    options.endpointTransport = chord_common::TransportType::Unix;
    options.agentPath = std::filesystem::path(CHORD_AGENT_EXECUTABLE);
    options.runDirectory = m_runner->getTesterDirectory();
    options.caKeyPair = m_caKeyPair;
    options.agentKeyPair = agentKeyPair;
    options.pemRootCABundleFile = m_caKeyPair.getPemCertificateFile();
    options.agentServerName = agentServerName;

    chord_sandbox::ChordIsolate isolate(options);

    // initialize the sandbox
    TU_RETURN_IF_NOT_OK (isolate.initialize());

    // construct the machine config
    absl::flat_hash_map<std::string,tempo_config::ConfigNode> config;
    std::vector<tempo_config::ConfigNode> packageDirectories;
    config["packageDirectories"] = tempo_config::startSeq()
        .append(tempo_config::valueNode(m_packageCache->getCacheDirectory().string()))
        .buildNode();
    config["pemRootCABundleFile"] = tempo_config::valueNode(options.pemRootCABundleFile.string());

    auto programName = tempo_utils::generate_name("program-XXXXXXXX");

    // run the module in the sandbox
    std::shared_ptr<chord_sandbox::RemoteMachine> remoteMachine;
    TU_ASSIGN_OR_RETURN (remoteMachine, isolate.spawn(
        programName, mainLocation, tempo_config::ConfigMap(config), m_options.protocolPlugs));

    TU_LOG_INFO << "spawned remote machine";

    // block until the remote machine is finished
    chord_sandbox::MachineExit machineExit;
    TU_ASSIGN_OR_RETURN (machineExit, remoteMachine->runUntilFinished());

    // return the interpreter result
    return SpawnMachine(m_runner, mainLocation, machineExit);
}

tempo_utils::Result<chord_test::SpawnMachine>
chord_test::ChordSandboxTester::spawnProgramInSandbox(const std::filesystem::path &packagePath)
{
    std::shared_ptr<zuri_packager::PackageReader> reader;
    TU_ASSIGN_OR_RETURN (reader, zuri_packager::PackageReader::open(packagePath));
    TU_RETURN_IF_STATUS (m_packageCache->installPackage(reader));
    zuri_packager::PackageSpecifier specifier;
    TU_ASSIGN_OR_RETURN (specifier, reader->readPackageSpecifier());
    return spawnProgramInSandbox(specifier.toUrl());
}

tempo_utils::Result<zuri_packager::PackageSpecifier>
chord_test::ChordSandboxTester::makePackage(const lyric_build::TargetComputation &targetComputation)
{
    auto *builder = m_runner->getBuilder();
    auto cache = builder->getCache();
    auto mainModulePath = targetComputation.getId().getId();
    auto taskState = targetComputation.getState();

    lyric_build::MetadataWriter objectFilterWriter;
    TU_RETURN_IF_NOT_OK (objectFilterWriter.configure());
    objectFilterWriter.putAttr(lyric_build::kLyricBuildContentType, std::string(lyric_common::kObjectContentType));
    lyric_build::LyricMetadata objectFilter;
    TU_ASSIGN_OR_RETURN(objectFilter, objectFilterWriter.toMetadata());

    std::vector<lyric_build::ArtifactId> artifactsFound;

    TU_ASSIGN_OR_RETURN (artifactsFound, cache->findArtifacts(
        taskState.getGeneration(), taskState.getHash(), {}, objectFilter));

    auto name = tempo_utils::generate_name("testXXXXXXXX");
    zuri_packager::PackageSpecifier specifier(name, "chord-test", 0, 0, 1);
    zuri_packager::PackageWriter writer(specifier);

    TU_RETURN_IF_NOT_OK (writer.configure());

    for (const auto &artifactId : artifactsFound) {
        lyric_build::LyricMetadata metadata;
        TU_ASSIGN_OR_RETURN (metadata, cache->loadMetadataFollowingLinks(artifactId));

        std::shared_ptr<const tempo_utils::ImmutableBytes> content;
        TU_ASSIGN_OR_RETURN (content,  cache->loadContentFollowingLinks(artifactId));

        auto modulePath = artifactId.getLocation().toPath();
        auto modulesRoot = tempo_utils::UrlPath::fromString("/modules");
        auto fullModulePath = modulesRoot.traverse(modulePath.toRelative());

        auto parentPath = fullModulePath.getInit();
        zuri_packager::EntryAddress parentEntry;
        TU_ASSIGN_OR_RETURN (parentEntry, writer.makeDirectory(parentPath, true));
        TU_RETURN_IF_STATUS (writer.putFile(fullModulePath, content));
    }

    tempo_config::ConfigMap packageConfig = tempo_config::startMap()
        .put("name", tempo_config::valueNode(specifier.getPackageName()))
        .put("version", tempo_config::valueNode(specifier.getVersionString()))
        .put("domain", tempo_config::valueNode(specifier.getPackageDomain()))
        .put("programMain", tempo_config::valueNode(mainModulePath))
        .buildMap();
    writer.setPackageConfig(packageConfig);

    std::filesystem::path packagePath;
    TU_ASSIGN_OR_RETURN (packagePath, writer.writePackage());
    TU_RETURN_IF_STATUS (m_packageCache->installPackage(packagePath));

    return specifier;
}

tempo_utils::Result<chord_test::RunMachine>
chord_test::ChordSandboxTester::runModuleInSandbox(
    const std::string &code,
    const std::filesystem::path &modulePath,
    const std::filesystem::path &baseDir)
{
    if (!m_runner->isConfigured())
        return TestStatus::forCondition(TestCondition::kTestInvariant,
            "tester is unconfigured");

    // write the code to a module file in the src directory
    lyric_common::ModuleLocation moduleLocation;
    TU_ASSIGN_OR_RETURN (moduleLocation, m_runner->writeModuleInternal(code, modulePath, baseDir));

    lyric_build::TaskId target("compile_module", moduleLocation.toString());

    // compile the module file
    lyric_build::TargetComputationSet targetComputationSet;
    TU_ASSIGN_OR_RETURN (targetComputationSet, m_runner->computeTargetInternal(target));

    auto targetComputation = targetComputationSet.getTarget(target);
    zuri_packager::PackageSpecifier specifier;
    TU_ASSIGN_OR_RETURN (specifier, makePackage(targetComputation));

    TU_CONSOLE_OUT << "";
    TU_CONSOLE_OUT << "======== RUN: " << specifier.toString() << " ========";
    TU_CONSOLE_OUT << "";

    auto agentDomain = tempo_utils::generate_name("chord-sandbox-tester-XXXXXXXX");
    auto agentServerName = absl::StrCat(agentDomain, ".test");
    auto generateAgentKeyPairResult = tempo_security::generate_key_pair(m_caKeyPair, m_eccKeygen,
        "Chord sandbox tester", "Chord agent", agentServerName,
        1, std::chrono::seconds{60},
        m_runner->getTesterDirectory(), "agent");
    if (generateAgentKeyPairResult.isStatus())
        return generateAgentKeyPairResult.getStatus();
    auto agentKeyPair = generateAgentKeyPairResult.getResult();

    // construct the sandbox
    chord_sandbox::SandboxOptions options;
    options.discoveryPolicy = chord_sandbox::AgentDiscoveryPolicy::ALWAYS_SPAWN;
    options.endpointTransport = chord_common::TransportType::Unix;
    options.agentPath = std::filesystem::path(CHORD_AGENT_EXECUTABLE);
    options.runDirectory = m_runner->getTesterDirectory();
    options.caKeyPair = m_caKeyPair;
    options.agentKeyPair = agentKeyPair;
    options.pemRootCABundleFile = m_caKeyPair.getPemCertificateFile();
    options.agentServerName = agentServerName;

    chord_sandbox::ChordIsolate isolate(options);

    // initialize the sandbox
    TU_RETURN_IF_NOT_OK (isolate.initialize());

    // construct the machine config
    absl::flat_hash_map<std::string,tempo_config::ConfigNode> config;
    config["packageDirectories"] = tempo_config::startSeq()
        .append(tempo_config::valueNode(m_packageCache->getCacheDirectory().string()))
        .buildNode();
    config["pemRootCABundleFile"] = tempo_config::ConfigValue(options.pemRootCABundleFile);

    auto programName = tempo_utils::generate_name("program-XXXXXXXX");

    // run the module in the sandbox
    std::shared_ptr<chord_sandbox::RemoteMachine> remoteMachine;
    TU_ASSIGN_OR_RETURN (remoteMachine, isolate.spawn(
        programName, specifier.toUrl(), tempo_config::ConfigMap(config), m_options.protocolPlugs));

    TU_LOG_INFO << "spawned remote machine";

    // block until the remote machine is finished
    chord_sandbox::MachineExit machineExit;
    TU_ASSIGN_OR_RETURN (machineExit, remoteMachine->runUntilFinished());

    // return the interpreter result
    return RunMachine(m_runner, targetComputation, targetComputationSet.getDiagnostics(), machineExit);
}

tempo_utils::Result<chord_test::SpawnMachine>
chord_test::ChordSandboxTester::spawnSingleProgramInSandbox(
    const tempo_utils::Url &mainLocation,
    const SandboxTesterOptions &options)
{
    ChordSandboxTester tester(options);
    TU_RETURN_IF_NOT_OK (tester.configure());
    return tester.spawnProgramInSandbox(mainLocation);
}

tempo_utils::Result<chord_test::SpawnMachine>
chord_test::ChordSandboxTester::spawnSingleProgramInSandbox(
    const std::filesystem::path &packagePath,
    const SandboxTesterOptions &options)
{
    ChordSandboxTester tester(options);
    TU_RETURN_IF_NOT_OK (tester.configure());
    return tester.spawnProgramInSandbox(packagePath);
}

tempo_utils::Result<chord_test::RunMachine>
chord_test::ChordSandboxTester::runSingleModuleInSandbox(
    const std::string &code,
    const SandboxTesterOptions &options)
{
    ChordSandboxTester tester(options);
    TU_RETURN_IF_NOT_OK (tester.configure());
    return tester.runModuleInSandbox(code);
}
