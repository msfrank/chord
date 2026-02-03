#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <chord_mesh/message.h>
#include <tempo_test/tempo_test.h>

#include "test_messages.capnp.h"

class Message : public ::testing::Test {};

TEST_F(Message, RoundTrip)
{
    chord_mesh::Message<test_generated::Request> input;
    auto inputRoot = input.getRoot();
    inputRoot.setValue("hello, world!");

    TU_CONSOLE_ERR << "input: " << input.toString();

    auto toBytesResult = input.toBytes();
    ASSERT_THAT (toBytesResult, tempo_test::IsResult());
    auto bytes = toBytesResult.getResult();

    chord_mesh::Message<test_generated::Request> output;
    ASSERT_THAT (output.parse(bytes), tempo_test::IsOk());
    TU_CONSOLE_ERR << "output: " << output.toString();

    auto outputRoot = output.getRoot();
    ASSERT_EQ ("hello, world!", outputRoot.getValue());
}