#include <iostream>

#include <chord_sandbox/chord_isolate.h>
#include <chord_test/chord_sandbox_tester.h>
#include <chord_test/test_result.h>
#include <lyric_build/build_result.h>
#include <lyric_runtime/bytecode_interpreter.h>
#include <lyric_test/test_inspector.h>
#include <tempo_security/ecc_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_utils/directory_maker.h>
#include <tempo_utils/file_utilities.h>

chord_test::ChordSandboxTester::ChordSandboxTester(const SandboxTesterOptions &options)
    : m_options(options),
      m_eccKeygen(NID_X9_62_prime256v1)
{
    m_runner = lyric_test::TestRunner::create(
        options.testRootDirectory,
        options.useInMemoryCache,
        options.isTemporary,
        options.keepBuildOnUnexpectedResult,
        options.preludeLocation,
        options.packageMap,
        options.buildConfig,
        options.buildVendorConfig);
}

tempo_utils::Status
chord_test::ChordSandboxTester::configure()
{
    auto status = m_runner->configureBaseTester();
    if (status.notOk())
        return status;

    m_domain = absl::StrCat(tempo_utils::generate_name("XXXXXXXX"), ".test");

    auto generateCAKeyPairResult = tempo_security::generate_self_signed_ca_key_pair(m_eccKeygen,
        "Chord sandbox tester", "Certificate authority", absl::StrCat("ca.", m_domain),
        1, std::chrono::minutes {60}, -1,
        m_runner->getTesterDirectory(), "ca");
    if (generateCAKeyPairResult.isStatus())
        return generateCAKeyPairResult.getStatus();
    m_caKeyPair = generateCAKeyPairResult.getResult();

    return {};
}

const lyric_test::TestRunner *
chord_test::ChordSandboxTester::getRunner() const
{
    return m_runner.get();
}

tempo_utils::Result<lyric_test::RunModule>
chord_test::ChordSandboxTester::runModuleInSandbox(const std::string &code, const std::filesystem::path &path)
{
    if (!m_runner->isConfigured())
        return TestStatus::forCondition(TestCondition::kTestInvariant,
            "tester is unconfigured");

    // write the code to a module file in the src directory
    auto writeSourceResult = m_runner->writeModuleInternal(code, path);
    if (writeSourceResult.isStatus())
        return writeSourceResult.getStatus();
    auto sourcePath = writeSourceResult.getResult();
    lyric_build::TaskId target("compile_module", sourcePath);

    // compile the module file
    auto buildResult = m_runner->computeTargetInternal(target, {}, {}, {});
    if (buildResult.isStatus())
        return buildResult.getStatus();
    auto targetComputationSet = buildResult.getResult();
    auto targetComputation = targetComputationSet.getTarget(target);
    TU_ASSERT (targetComputation.isValid());

    // construct module location based on the source path
    std::filesystem::path modulePath = sourcePath;
    modulePath.replace_extension();
    lyric_common::AssemblyLocation moduleLocation(modulePath.string());

    TU_CONSOLE_OUT << "";
    TU_CONSOLE_OUT << "======== RUN: " << moduleLocation << " ========";
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
    options.endpointTransport = chord_protocol::TransportType::Unix;
    options.agentPath = std::filesystem::path(CHORD_AGENT_EXECUTABLE);
    options.runDirectory = m_runner->getTesterDirectory();
    options.caKeyPair = m_caKeyPair;
    options.agentKeyPair = agentKeyPair;
    options.pemRootCABundleFile = m_caKeyPair.getPemCertificateFile();
    options.agentServerName = agentServerName;

    // initialize the sandbox
    chord_sandbox::ChordIsolate isolate(options);
    auto sandboxStatus = isolate.initialize();
    if (sandboxStatus.notOk())
        return sandboxStatus;

    // construct the machine config
    absl::flat_hash_map<std::string,tempo_config::ConfigNode> config;
    auto installDirectory = m_runner->getInstallDirectory() / "compile_module";
    config["installDirectory"] = tempo_config::ConfigValue(installDirectory.string());
    auto *builder = m_runner->getBuilder();
    auto packageLoader = builder->getPackageLoader();
    std::vector<tempo_config::ConfigNode> packageDirectories;
    for (const auto &packageDirectory : packageLoader->getPackagesPathList()) {
        packageDirectories.push_back(tempo_config::ConfigValue(packageDirectory.string()));
    }
    config["packageDirectories"] = tempo_config::ConfigSeq(packageDirectories);
    config["pemRootCABundleFile"] = tempo_config::ConfigValue(options.pemRootCABundleFile);

    // run the module in the sandbox
    auto spawnMachineResult = isolate.spawn(sourcePath.string(), moduleLocation,
        tempo_config::ConfigMap(config), m_options.requestedPorts, m_options.protocolPlugs,
        m_options.runProtocolCallback, m_options.runProtocolCallbackData);
    if (spawnMachineResult.isStatus())
        return spawnMachineResult.getStatus();

    TU_LOG_INFO << "spawned remote machine";

    // start the remote machine
    auto machine = spawnMachineResult.getResult();
    TU_RETURN_IF_NOT_OK (machine->start());

    // FIXME: get the execution result
    machine->runUntilFinished();

    // return the interpreter result
    return lyric_test::RunModule(m_runner, targetComputation,
        targetComputationSet.getDiagnostics(), lyric_runtime::DataCell::nil());
}

tempo_utils::Result<lyric_test::RunModule>
chord_test::ChordSandboxTester::runSingleModuleInSandbox(
    const std::string &code,
    const SandboxTesterOptions &options)
{
    chord_test::ChordSandboxTester tester(options);
    auto status = tester.configure();
    if (!status.isOk())
        return status;
    return tester.runModuleInSandbox(code);
}
