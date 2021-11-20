#include "rtc/base/memory/bit_io_reader.hpp"
#include "rtc/base/memory/byte_io_writer.hpp"

#include <gtest/gtest.h>

#define ENABLE_UNIT_TESTS 0
#include "../testing/unittest_defines.hpp"

#include <vector>

namespace naivertc {
namespace test {

MY_TEST(BitReaderTest, ConsumeBits) {
    const uint8_t bytes[64] = {0};
    BitReader bit_reader(bytes, 32);
    uint64_t total_bits = 32 * 8;
    EXPECT_EQ(total_bits, bit_reader.RemainingBitCount());
    EXPECT_TRUE(bit_reader.ConsumeBits(3));
    total_bits -= 3;
    EXPECT_EQ(total_bits, bit_reader.RemainingBitCount());
    EXPECT_TRUE(bit_reader.ConsumeBits(3));
    total_bits -= 3;
    EXPECT_EQ(total_bits, bit_reader.RemainingBitCount());
    EXPECT_TRUE(bit_reader.ConsumeBits(15));
    total_bits -= 15;
    EXPECT_EQ(total_bits, bit_reader.RemainingBitCount());
    EXPECT_TRUE(bit_reader.ConsumeBits(37));
    total_bits -= 37;
    EXPECT_EQ(total_bits, bit_reader.RemainingBitCount());

    EXPECT_FALSE(bit_reader.ConsumeBits(32 * 8));
    EXPECT_EQ(total_bits, bit_reader.RemainingBitCount());
}

MY_TEST(BitReaderTest, ReadByteAligned) {
    const uint8_t bytes[] = {0x0A, 0xBC, 0xDE, 0xF1, 0x23, 0x45, 0x67, 0x89};
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;
    BitReader bit_reader(bytes, 8);
    EXPECT_TRUE(bit_reader.ReadByte(val8));
    EXPECT_EQ(0x0Au, val8);
    EXPECT_TRUE(bit_reader.ReadByte(val8));
    EXPECT_EQ(0xBCu, val8);
    EXPECT_TRUE(bit_reader.ReadByte(val16));
    EXPECT_EQ(0xDEF1u, val16);
    EXPECT_TRUE(bit_reader.ReadByte(val32));
    EXPECT_EQ(0x23456789u, val32);
}

MY_TEST(BitReaderTest, ReadBytesOffset4) {
    const uint8_t bytes[] = {0x0A, 0xBC, 0xDE, 0xF1, 0x23,
                            0x45, 0x67, 0x89, 0x0A};
    uint8_t val8;
    uint16_t val16;
    uint32_t val32;
    BitReader bit_reader(bytes, 9);
    EXPECT_TRUE(bit_reader.ConsumeBits(4));

    EXPECT_TRUE(bit_reader.ReadByte(val8));
    EXPECT_EQ(0xABu, val8);
    EXPECT_TRUE(bit_reader.ReadByte(val8));
    EXPECT_EQ(0xCDu, val8);
    EXPECT_TRUE(bit_reader.ReadByte(val16));
    EXPECT_EQ(0xEF12u, val16);
    EXPECT_TRUE(bit_reader.ReadByte(val32));
    EXPECT_EQ(0x34567890u, val32);
}

MY_TEST(BitReaderTest, ReadBytesOffset3) {
    // The pattern we'll check against is counting down from 0b1111. It looks
    // weird here because it's all offset by 3.
    // Byte pattern is:
    //    56701234
    //  0b00011111,
    //  0b11011011,
    //  0b10010111,
    //  0b01010011,
    //  0b00001110,
    //  0b11001010,
    //  0b10000110,
    //  0b01000010
    //       xxxxx <-- last 5 bits unused.

    // The bytes. It almost looks like counting down by two at a time, except the
    // jump at 5->3->0, since that's when the high bit is turned off.
    const uint8_t bytes[] = {0x1F, 0xDB, 0x97, 0x53, 0x0E, 0xCA, 0x86, 0x42};

    uint8_t val8;
    uint16_t val16;
    uint32_t val32;
    BitReader bit_reader(bytes, 8);
    EXPECT_TRUE(bit_reader.ConsumeBits(3));
    EXPECT_TRUE(bit_reader.ReadByte(val8));
    EXPECT_EQ(0xFEu, val8);
    EXPECT_TRUE(bit_reader.ReadByte(val16));
    EXPECT_EQ(0xDCBAu, val16);
    EXPECT_TRUE(bit_reader.ReadByte(val32));
    EXPECT_EQ(0x98765432u, val32);
    // 5 bits left unread. Not enough to read a uint8_t.
    EXPECT_EQ(5u, bit_reader.RemainingBitCount());
    EXPECT_FALSE(bit_reader.ReadByte(val8));
}

MY_TEST(BitReaderTest, ReadBits) {
    // Bit values are:
    //  0b01001101,
    //  0b00110010
    const uint8_t bytes[] = {0x4D, 0x32};
    uint32_t val;
    BitReader bit_reader(bytes, 2);
    EXPECT_TRUE(bit_reader.ReadBits(3, val));
    // 0b010
    EXPECT_EQ(0x2u, val);
    EXPECT_TRUE(bit_reader.ReadBits(2, val));
    // 0b01
    EXPECT_EQ(0x1u, val);
    EXPECT_TRUE(bit_reader.ReadBits(7, val));
    // 0b1010011
    EXPECT_EQ(0x53u, val);
    EXPECT_TRUE(bit_reader.ReadBits(2, val));
    // 0b00
    EXPECT_EQ(0x0u, val);
    EXPECT_TRUE(bit_reader.ReadBits(1, val));
    // 0b1
    EXPECT_EQ(0x1u, val);
    EXPECT_TRUE(bit_reader.ReadBits(1, val));
    // 0b0
    EXPECT_EQ(0x0u, val);

    EXPECT_FALSE(bit_reader.ReadBits(1, val));
}

MY_TEST(BitReaderTest, ReadBits64) {
    const uint8_t bytes[] = {0x4D, 0x32, 0xAB, 0x54, 0x00, 0xFF, 0xFE, 0x01,
                            0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89};
    BitReader bit_reader(bytes, 16);
    uint64_t val;

    // Peek and read first 33 bits.
    EXPECT_TRUE(bit_reader.PeekBits(33, val));
    EXPECT_EQ(0x4D32AB5400FFFE01ull >> (64 - 33), val);
    val = 0;
    EXPECT_TRUE(bit_reader.ReadBits(33, val));
    EXPECT_EQ(0x4D32AB5400FFFE01ull >> (64 - 33), val);

    // Peek and read next 31 bits.
    constexpr uint64_t kMask31Bits = (1ull << 32) - 1;
    EXPECT_TRUE(bit_reader.PeekBits(31, val));
    EXPECT_EQ(0x4D32AB5400FFFE01ull & kMask31Bits, val);
    val = 0;
    EXPECT_TRUE(bit_reader.ReadBits(31, val));
    EXPECT_EQ(0x4D32AB5400FFFE01ull & kMask31Bits, val);

    // Peek and read remaining 64 bits.
    EXPECT_TRUE(bit_reader.PeekBits(64, val));
    EXPECT_EQ(0xABCDEF0123456789ull, val);
    val = 0;
    EXPECT_TRUE(bit_reader.ReadBits(64, val));
    EXPECT_EQ(0xABCDEF0123456789ull, val);

    // Nothing more to read.
    EXPECT_FALSE(bit_reader.ReadBits(1, val));
}

uint64_t GolombEncoded(uint32_t val) {
    val++;
    uint32_t bit_counter = val;
    uint64_t bit_count = 0;
    while (bit_counter > 0) {
        bit_count++;
        bit_counter >>= 1;
    }
    return static_cast<uint64_t>(val) << (64 - (bit_count * 2 - 1));
}

MY_TEST(BitReaderTest, GolombUint32Values) {
    std::vector<uint8_t> byteBuffer;
    byteBuffer.resize(16);
    BitReader bit_reader(reinterpret_cast<const uint8_t*>(byteBuffer.data()), byteBuffer.capacity());
    // Test over the uint32_t range with a large enough step that the test doesn't
    // take forever. Around 20,000 iterations should do.
    const int kStep = std::numeric_limits<uint32_t>::max() / 20000;
    for (uint32_t i = 0; i < std::numeric_limits<uint32_t>::max() - kStep;
        i += kStep) {
        uint64_t encoded_val = GolombEncoded(i);
        byteBuffer.clear();
        ByteWriter<uint64_t>::WriteBigEndian(byteBuffer.data(), encoded_val);
        uint32_t decoded_val;
        EXPECT_TRUE(bit_reader.Seek(0, 0));
        EXPECT_TRUE(bit_reader.ReadExpGolomb(decoded_val));
        EXPECT_EQ(i, decoded_val);
    }
}

MY_TEST(BitReaderTest, SignedGolombValues) {
    uint8_t golomb_bits[] = {
        0x80,  // 1
        0x40,  // 010
        0x60,  // 011
        0x20,  // 00100
        0x38,  // 00111
    };
    int32_t expected[] = {0, 1, -1, 2, -3};
    for (size_t i = 0; i < sizeof(golomb_bits); ++i) {
        BitReader bit_reader(&golomb_bits[i], 1);
        int32_t decoded_val;
        ASSERT_TRUE(bit_reader.ReadSignedExpGolomb(decoded_val));
        EXPECT_EQ(expected[i], decoded_val)
            << "Mismatch in expected/decoded value for golomb_bits[" << i
            << "]: " << static_cast<int>(golomb_bits[i]);
    }
}

MY_TEST(BitReaderTest, NoGolombOverread) {
    const uint8_t bytes[] = {0x00, 0xFF, 0xFF};
    // Make sure the bit bit_reader correctly enforces byte length on golomb reads.
    // If it didn't, the above bit_reader would be valid at 3 bytes.
    BitReader bit_reader(bytes, 1);
    uint32_t decoded_val;
    EXPECT_FALSE(bit_reader.ReadExpGolomb(decoded_val));

    BitReader longer_bit_reader(bytes, 2);
    EXPECT_FALSE(longer_bit_reader.ReadExpGolomb(decoded_val));

    BitReader longest_bit_reader(bytes, 3);
    EXPECT_TRUE(longest_bit_reader.ReadExpGolomb(decoded_val));
    // Golomb should have read 9 bits, so 0x01FF, and since it is golomb, the
    // result is 0x01FF - 1 = 0x01FE.
    EXPECT_EQ(0x01FEu, decoded_val);
}
    
} // namespace test
} // namespace naivertc
