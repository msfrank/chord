#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_sandbox/internal/spawn_utils.h>

TEST(SpawnUtils, SpawnTemporaryAgent)
{
    // chord_sandbox::internal::AgentParams agentParams;
    //
    // std::filesystem::path agentPath("agentPath");
    // chord_common::TransportType transport = chord_common::TransportType::Unix;
    // std::string agentServerName("servername");
    // std::filesystem::path runDirectory("runDirectory");
    // std::filesystem::path pemCertificateFile("pemCertificateFile");
    // std::filesystem::path pemPrivateKeyFile("pemPrivateKeyFile");
    // std::filesystem::path pemRootCABundleFile("pemRootCABundleFile");
    // std::string childOutput("/path/to/sock");
    // int exitStatus = 0;
    //
    // auto spawnFunc = [&](const tempo_utils::ProcessInvoker &invoker_, const std::filesystem::path &runDirectory_) {
    //     return std::make_shared<tempo_utils::DaemonProcess>(invoker_, runDirectory_, childOutput, exitStatus);
    // };
    //
    // auto status = chord_sandbox::internal::spawn_temporary_agent(
    //     agentParams, agentPath, transport, agentServerName,
    //     runDirectory, pemCertificateFile, pemPrivateKeyFile, pemRootCABundleFile);
    // ASSERT_TRUE (status.isOk());
    //
    // ASSERT_EQ (childOutput, agentParams.agentEndpoint);
    // ASSERT_EQ (agentServerName, agentParams.agentServerName);
    // ASSERT_EQ (pemRootCABundleFile, agentParams.pemRootCABundleFile);
    //
    // auto process = agentParams.agentProcess;
    // ASSERT_EQ (runDirectory, process->getRunDirectory());
    // ASSERT_EQ (childOutput, process->getChildOutput());
    // ASSERT_EQ (exitStatus, process->getExitStatus());
}