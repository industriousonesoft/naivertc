#include "rtc/base/memory/bit_io_writer.hpp"
#include "rtc/base/memory/bit_io_reader.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "testing/defines.hpp"

namespace naivertc {
namespace test {

MY_TEST(BitWriterTest, SetOffsetValues) {
    uint8_t bytes[4] = {0};
    BitWriter bit_writer(bytes, 4);

    size_t byte_offset, bit_offset;
    // Bit offsets are [0,7].
    EXPECT_TRUE(bit_writer.Seek(0, 0));
    EXPECT_TRUE(bit_writer.Seek(0, 7));
    bit_writer.GetCurrentOffset(&byte_offset, &bit_offset);
    EXPECT_EQ(0u, byte_offset);
    EXPECT_EQ(7u, bit_offset);
    EXPECT_FALSE(bit_writer.Seek(0, 8));
    bit_writer.GetCurrentOffset(&byte_offset, &bit_offset);
    EXPECT_EQ(0u, byte_offset);
    EXPECT_EQ(7u, bit_offset);
    // Byte offsets are [0,length]. At byte offset length, the bit offset must be
    // 0.
    EXPECT_TRUE(bit_writer.Seek(0, 0));
    EXPECT_TRUE(bit_writer.Seek(2, 4));
    bit_writer.GetCurrentOffset(&byte_offset, &bit_offset);
    EXPECT_EQ(2u, byte_offset);
    EXPECT_EQ(4u, bit_offset);
    EXPECT_TRUE(bit_writer.Seek(4, 0));
    EXPECT_FALSE(bit_writer.Seek(5, 0));
    bit_writer.GetCurrentOffset(&byte_offset, &bit_offset);
    EXPECT_EQ(4u, byte_offset);
    EXPECT_EQ(0u, bit_offset);
    EXPECT_FALSE(bit_writer.Seek(4, 1));
}

MY_TEST(BitWriterTest, SymmetricGolomb) {
    char test_string[] = "hello,world";
    uint8_t bytes[64] = {0};
    size_t w_byte_offset, w_bit_offset;
    size_t r_byte_offset, r_bit_offset;
    BitWriter bit_writer(bytes, 64);
    BitReader bit_reader(bytes, 64);
    for (size_t i = 0; i < 11; ++i) {
        EXPECT_TRUE(bit_writer.WriteExpGolomb(test_string[i]));
        bit_writer.GetCurrentOffset(&w_byte_offset, &w_bit_offset);
        
        uint32_t val;
        EXPECT_TRUE(bit_reader.ReadExpGolomb(val));
        bit_reader.GetCurrentOffset(&r_byte_offset, &r_bit_offset);
        EXPECT_EQ(test_string[i], static_cast<char>(val));

        EXPECT_EQ(w_byte_offset, r_byte_offset);
        EXPECT_EQ(w_bit_offset, r_bit_offset);
    }
    
    bit_reader.Seek(0,0);
    for (size_t i = 0; i < 11; ++i) {
        uint32_t val;
        EXPECT_TRUE(bit_reader.ReadExpGolomb(val));
        EXPECT_LE(val, std::numeric_limits<uint8_t>::max());
        EXPECT_EQ(test_string[i], static_cast<char>(val));
    }
}
    
} // namespace test
} // namespace naivertc
