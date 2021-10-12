#include "rtc/rtp_rtcp/rtcp/rtcp_packets/common_header.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

using namespace naivertc::rtcp;

TEST(RtcpCommonHeaderTest, TooSmallBuffer) {
    const uint8_t buffer[] = {0x80, 0x00, 0x00, 0x00};
    CommonHeader header;
    EXPECT_FALSE(header.Parse(buffer, 0));
    EXPECT_FALSE(header.Parse(buffer, 1));
    EXPECT_FALSE(header.Parse(buffer, 2));
    EXPECT_FALSE(header.Parse(buffer, 3));
    EXPECT_TRUE(header.Parse(buffer, 4));
}

TEST(RtcpCommonHeaderTest, Version) {
    uint8_t buffer[] = {0x00, 0x00, 0x00, 0x00};
    size_t buffer_size = 4;
    CommonHeader header;
    // Version 2 is the only allowed.
    buffer[0] = 0 << 6;
    EXPECT_FALSE(header.Parse(buffer, buffer_size));
    buffer[0] = 1 << 6;
    EXPECT_FALSE(header.Parse(buffer, buffer_size));
    buffer[0] = 2 << 6;
    EXPECT_TRUE(header.Parse(buffer, buffer_size));
    buffer[0] = 3 << 6;
    EXPECT_FALSE(header.Parse(buffer, buffer_size));
}

TEST(RtcpCommonHeaderTest, PacketSize) {
    uint8_t buffer[] = {0x80, 0x00, 0x00, 0x02, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    size_t buffer_size = 12;
    CommonHeader header;
    EXPECT_FALSE(header.Parse(buffer, buffer_size - 1));
    EXPECT_TRUE(header.Parse(buffer, buffer_size));
    EXPECT_EQ(8u, header.payload_size());
    EXPECT_EQ(buffer + buffer_size, header.NextPacket());
    EXPECT_EQ(buffer_size, header.packet_size());
}

TEST(RtcpCommonHeaderTest, PaddingAndPayloadSize) {
    // Set v = 2, p = 1, but leave fmt, pt as 0.
    uint8_t buffer[] = {0xa0, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    size_t buffer_size = 12;
    CommonHeader header;
    // Padding bit set, but no byte for padding (can't specify padding length).
    EXPECT_FALSE(header.Parse(buffer, 4));

    buffer[3] = 2;  //  Set payload size to 2x32bit.
    const size_t kPayloadSizeBytes = buffer[3] * 4;
    const size_t kPaddingAddress =
        CommonHeader::kFixedHeaderSize + kPayloadSizeBytes - 1;

    // Padding one byte larger than possible.
    buffer[kPaddingAddress] = kPayloadSizeBytes + 1;
    EXPECT_FALSE(header.Parse(buffer, buffer_size));

    // Invalid zero padding size.
    buffer[kPaddingAddress] = 0;
    EXPECT_FALSE(header.Parse(buffer, buffer_size));

    // Pure padding packet.
    buffer[kPaddingAddress] = kPayloadSizeBytes;
    EXPECT_TRUE(header.Parse(buffer, buffer_size));
    EXPECT_EQ(0u, header.payload_size());
    EXPECT_EQ(buffer + buffer_size, header.NextPacket());
    EXPECT_EQ(header.payload(), buffer + CommonHeader::kFixedHeaderSize);
    EXPECT_EQ(header.packet_size(), buffer_size);

    // Single byte of actual data.
    buffer[kPaddingAddress] = kPayloadSizeBytes - 1;
    EXPECT_TRUE(header.Parse(buffer, buffer_size));
    EXPECT_EQ(1u, header.payload_size());
    EXPECT_EQ(buffer + buffer_size, header.NextPacket());
    EXPECT_EQ(header.packet_size(), buffer_size);
}

TEST(RtcpCommonHeaderTest, FormatAndPayloadType) {
    const uint8_t buffer[] = {0x9e, 0xab, 0x00, 0x00};
    size_t buffer_size = 4;
    CommonHeader header;
    EXPECT_TRUE(header.Parse(buffer, buffer_size));

    EXPECT_EQ(header.count(), 0x1e);
    EXPECT_EQ(header.feedback_message_type(), 0x1E);
    EXPECT_EQ(header.type(), 0xab);
    EXPECT_EQ(header.payload_size(), 0U);
    EXPECT_EQ(header.payload(), buffer + CommonHeader::kFixedHeaderSize);
}
    
} // namespace test
} // namespace naivertc
