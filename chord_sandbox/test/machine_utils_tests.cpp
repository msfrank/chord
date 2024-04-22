#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_invoke/invoke_service_mock.grpc.pb.h>
#include <chord_sandbox/internal/machine_utils.h>
#include <tempo_config/config_serde.h>
#include <tempo_security/ecc_private_key_generator.h>
#include <tempo_security/generate_utils.h>
#include <tempo_security/x509_certificate_signing_request.h>
#include <tempo_security/x509_store.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/file_writer.h>
#include <tempo_utils/tempdir_maker.h>

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SaveArg;

TEST(MachineUtils, CreateMachineSucceeds)
{
    chord_invoke::MockInvokeServiceStub stub;
    chord_invoke::CreateMachineRequest createMachineRequest;
    chord_invoke::CreateMachineResult createMachineResult;

    EXPECT_CALL(stub, CreateMachine(_,_,_))
        .Times(1)
        .WillOnce(DoAll(
            SaveArg<1>(&createMachineRequest),
            SetArgPointee<2>(createMachineResult),
            Return(grpc::Status::OK)));

    std::string name("foo");
    auto executionUrl = tempo_utils::Url::fromString("/module");

    tempo_config::ConfigMap config({
        {"foo", tempo_config::ConfigValue("bar")}
    });

    auto portUrl = tempo_utils::Url::fromString("dev.zuri.proto:null");
    chord_protocol::RequestedPort port(portUrl, chord_protocol::PortType::Streaming,
        chord_protocol::PortDirection::BiDirectional);
    absl::flat_hash_set<chord_protocol::RequestedPort> requestedPorts = {port};

    chord_invoke::CreateMachineResult resultReturned;
    TU_ASSIGN_OR_RAISE (resultReturned, chord_sandbox::internal::create_machine(&stub,
        "foo", executionUrl, config, requestedPorts));

    ASSERT_EQ (name, createMachineRequest.name());
    ASSERT_EQ (executionUrl.toString(), createMachineRequest.execution_uri());

    tempo_config::ConfigNode config_;
    TU_ASSIGN_OR_RAISE (config_, tempo_config::read_config_string(createMachineRequest.config_hash()));
    ASSERT_EQ (config_, config);

    const auto &requested_ports = createMachineRequest.requested_ports();
    ASSERT_EQ (1, requested_ports.size());
    const auto &requested_port = requested_ports.at(0);
    ASSERT_EQ (portUrl.toString(), requested_port.protocol_uri());
    ASSERT_EQ (chord_invoke::PortDirection::BiDirectional, requested_port.port_direction());
    ASSERT_EQ (chord_invoke::PortType::Streaming, requested_port.port_type());
}

TEST(MachineUtils, RunMachineSucceeds)
{
    tempo_utils::TempdirMaker tempdirMaker("tester.XXXXXXXX");
    ASSERT_TRUE (tempdirMaker.isValid());
    auto testerDirectory = tempdirMaker.getTempdir();

    std::string organization("test");
    auto organizationalUnit = tempo_utils::generate_name("XXXXXXXX");
    auto caCommonName = absl::StrCat("ca.", organizationalUnit, ".", organization);

    tempo_security::ECCPrivateKeyGenerator keygen(NID_X9_62_prime256v1);

    auto generateCAKeyPairResult = tempo_security::generate_self_signed_ca_key_pair(keygen,
        organization, organizationalUnit, caCommonName,
        1, std::chrono::seconds{60}, -1,
        testerDirectory, "ca");
    ASSERT_TRUE (generateCAKeyPairResult.isResult());
    auto caKeyPair = generateCAKeyPairResult.getResult();

    chord_invoke::MockInvokeServiceStub stub;
    chord_invoke::RunMachineRequest runMachineRequest;
    chord_invoke::RunMachineResult runMachineResult;

    EXPECT_CALL(stub, RunMachine(_,_,_))
        .Times(1)
        .WillOnce(DoAll(
            SaveArg<1>(&runMachineRequest),
            SetArgPointee<2>(runMachineResult),
            Return(grpc::Status::OK)));

    auto machineUrl = tempo_utils::Url::fromString("/machine");

    std::string endpointCommonName("foo");

    auto generateCsrKeyPairResult = tempo_security::generate_csr_key_pair(keygen,
        organization, organizationalUnit, endpointCommonName,
        testerDirectory, endpointCommonName);
    ASSERT_TRUE (generateCsrKeyPairResult.isResult());
    auto csrKeyPair = generateCsrKeyPairResult.getResult();
    tempo_utils::FileReader csrReader(csrKeyPair.getPemRequestFile());
    ASSERT_TRUE (csrReader.isValid());
    auto csrBytes = csrReader.getBytes();

    absl::flat_hash_map<std::string,std::string> declaredEndpointCsrs;
    declaredEndpointCsrs[endpointCommonName] = std::string((const char *) csrBytes->getData(), csrBytes->getSize());

    chord_invoke::RunMachineResult resultReturned;
    TU_ASSIGN_OR_RAISE (resultReturned, chord_sandbox::internal::run_machine(&stub, machineUrl,
        declaredEndpointCsrs, caKeyPair, std::chrono::seconds(3600)));

    ASSERT_EQ (machineUrl.toString(), runMachineRequest.machine_uri());

    const auto &signed_endpoints = runMachineRequest.signed_endpoints();
    ASSERT_EQ (1, signed_endpoints.size());
    const auto &signed_endpoint = signed_endpoints.at(0);
    ASSERT_EQ (endpointCommonName, signed_endpoint.endpoint_uri());

    tempo_utils::FileWriter certWriter(testerDirectory / absl::StrCat(endpointCommonName,".cert"),
        signed_endpoint.certificate(), tempo_utils::FileWriterMode::CREATE_ONLY);
    ASSERT_TRUE (certWriter.isValid());

    tempo_security::X509StoreOptions options;
    options.depth = 0;

    std::shared_ptr<tempo_security::X509Store> x509Store;
    TU_ASSIGN_OR_RAISE(x509Store, tempo_security::X509Store::loadLocations(
        options, {}, caKeyPair.getPemCertificateFile()));

    auto status = x509Store->verifyCertificate(certWriter.getAbsolutePath());
    ASSERT_TRUE (status.isOk());
}