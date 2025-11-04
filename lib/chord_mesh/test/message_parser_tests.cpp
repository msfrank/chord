#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_mesh/message.h>
#include <tempo_test/tempo_test.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/tempdir_maker.h>

#include "base_mesh_fixture.h"

class MessageParser : public BaseMeshFixture {};

TEST_F(MessageParser, ParseMessage)
{
    chord_mesh::MessageBuilder builder;
    auto payload = tempo_utils::MemoryBytes::copy("hello, world!");
    builder.setPayload(payload);
    auto toBytesResult = builder.toBytes();
    ASSERT_THAT (toBytesResult, tempo_test::IsResult());
    auto bytes = toBytesResult.getResult();

    chord_mesh::MessageParser parser;
    ASSERT_TRUE (parser.appendBytes(bytes->getSpan()));
    ASSERT_TRUE (parser.hasMessage());
    auto message = parser.takeMessage();

    std::string_view payloadString((const char *) payload->getData(), payload->getSize());
    std::string_view messageString((const char *) message->getData(), message->getSize());
    ASSERT_EQ (payloadString, messageString);
}
