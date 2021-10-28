#include "rtc/rtp_rtcp/rtp/fec/fec_header_reader_ulp.hpp"

#include <gtest/gtest.h>

#include <memory>

namespace naivertc {
namespace test {

TEST(UlpFecHeaderReaderTest, ReadFecHeaderWithLBitClear) {
    const uint8_t packet[] = {
        // Level0
        0x00, 0x12, 0xab, 0xcd,  // L bit clear, "random" payload type and SN base
        0x12, 0x34, 0x56, 0x78,  // "random" TS recovery
        0xab, 0xcd,              // "random" length recovery
        // Level1
        0x11, 0x22, 0x33, 0x44,  // "random"  protection length and packet mask

        0x00, 0x00, 0x00, 0x00   // payload
    };
    const size_t packet_size = sizeof(packet);
    CopyOnWriteBuffer fec_packet(packet, packet_size);
    UlpFecHeaderReader reader;
    FecHeader fec_header;
    EXPECT_TRUE(reader.ReadFecHeader(fec_header, fec_packet));

    EXPECT_EQ(14u, fec_header.fec_header_size);
    EXPECT_EQ(0xabcdu, fec_header.seq_num_base);
    EXPECT_EQ(12u, fec_header.packet_mask_offset);
    EXPECT_EQ(2u, fec_header.packet_mask_size);
    EXPECT_EQ(0x1122u, fec_header.protection_length);
}


TEST(UlpFecHeaderReaderTest, ReadFecHeaderWithLBitSet) {
    const uint8_t packet[] = {
        // Leve0
        0x40, 0x12, 0xab, 0xcd,  // L bit clear, "random" payload type and SN base
        0x12, 0x34, 0x56, 0x78,  // "random" TS recovery
        0xab, 0xcd,              // "random" length recovery
        // Leve1
        0x11, 0x22, 0x33, 0x44,  // "random" protection length and packet mask
        0x55, 0x66, 0x77, 0x88,

        0x00, 0x00, 0x00, 0x00   // payload
    };
    const size_t packet_size = sizeof(packet);
    CopyOnWriteBuffer fec_packet(packet, packet_size);
    UlpFecHeaderReader reader;
    FecHeader fec_header;
    EXPECT_TRUE(reader.ReadFecHeader(fec_header, fec_packet));

    EXPECT_EQ(18u, fec_header.fec_header_size);
    EXPECT_EQ(0xabcdu, fec_header.seq_num_base);
    EXPECT_EQ(12u, fec_header.packet_mask_offset);
    EXPECT_EQ(6u, fec_header.packet_mask_size);
    EXPECT_EQ(0x1122u, fec_header.protection_length);
}

} // namespace test
} // namespace naivertc
