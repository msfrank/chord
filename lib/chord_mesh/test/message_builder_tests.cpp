#include <gtest/gtest.h>
#include <gmock/gmock.h>


#include <chord_mesh/message.h>
#include <tempo_test/tempo_test.h>
#include <tempo_utils/big_endian.h>
#include <tempo_utils/file_utilities.h>
#include <tempo_utils/tempdir_maker.h>

#include "base_mesh_fixture.h"

class MessageBuilder : public BaseMeshFixture {};

TEST_F(MessageBuilder, BuildMessage)
{
    chord_mesh::MessageBuilder builder;
    builder.setPayload(tempo_utils::MemoryBytes::copy("hello, world!"));
    auto toBytesResult = builder.toBytes();
    ASSERT_THAT (toBytesResult, tempo_test::IsResult());
    auto bytes = toBytesResult.getResult();
    ASSERT_LE (5, bytes->getSize());
    auto *ptr = bytes->getData();
    tu_uint8 version = tempo_utils::read_u8_and_advance(ptr);
    ASSERT_EQ (1, version);
    tu_uint32 size = tempo_utils::read_u32_and_advance(ptr);
    ASSERT_EQ (13, size);
    std::string_view payload((const char *) ptr, size);
    ASSERT_EQ ("hello, world!", payload);
}
