#include "rtc/rtp_rtcp/base/rtp_packet.hpp"

#include <gtest/gtest.h>

namespace naivertc {
namespace test {

TEST(RtpPacketTest, BuildPacket) {
    auto packet = RtpPacket::Create();
    EXPECT_NE(packet, nullptr);
    EXPECT_EQ(packet->capacity(), 1500);
    packet->set_marker(true);
    packet->set_payload_type(0x0Fu);
    packet->set_sequence_number(0x06FD);
    packet->set_timestamp(0xF6E8F500);
    packet->set_ssrc(0x00123445);

    EXPECT_EQ(packet->marker(), true);
    EXPECT_EQ(packet->has_padding(), false);
    EXPECT_EQ(packet->padding_size(), 0);
    EXPECT_EQ(packet->payload_type(), 0x0Fu);
    EXPECT_EQ(packet->sequence_number(), 0x06FD);
    EXPECT_EQ(packet->timestamp(), 0xF6E8F500);
    EXPECT_EQ(packet->ssrc(), 0x00123445);
    EXPECT_EQ(packet->header_size(), 12);

    EXPECT_EQ(packet->payload_size(), 0);
}

    
} // namespace test
} // namespace naivertc 