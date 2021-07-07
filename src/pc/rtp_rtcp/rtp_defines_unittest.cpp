#include "pc/rtp_rtcp/rtp_defines.hpp"

#include <gtest/gtest.h>

#include <memory>

using namespace testing; 

namespace naivertc {
namespace test {

// RTP packet
TEST(RtpRtcpTest, ParseRTPPacket) {
    const uint8_t raw_bytes[] = {0x80, 0xe0, 0x00, 0x1e, 0x00, 0x00, 0xd2, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x41, 0x9b, 0x6b, 0x49, 0xe1, 0x0f, 0x26, 0x53, 0x02, 0x1a, 0xff, 0x06, 0x59, 0x97, 0x1d, 0xd2, 0x2e, 0x8c, 0x50, 0x01};

    struct RTP rtp_packet;
    
    memcpy(&rtp_packet, raw_bytes, sizeof(raw_bytes)); 

    EXPECT_EQ(rtp_packet.version(), 0x02);
    EXPECT_EQ(rtp_packet.padding(), false);
    EXPECT_EQ(rtp_packet.extension(), false);
    EXPECT_EQ(rtp_packet.csrc_count(), 0);

    EXPECT_EQ(rtp_packet.marker(), true);
    EXPECT_EQ(rtp_packet.payload_type(), 0x60 /* 96 */);
    EXPECT_EQ(rtp_packet.seq_number(), 0x001e);
    EXPECT_EQ(rtp_packet.timestamp(), 0x0000d2f0);

    EXPECT_EQ(rtp_packet.ssrc(), 0x00000000);
    EXPECT_EQ(rtp_packet.header_size(), 12);

}

TEST(RtpRtcpTest, CreateRTPPacket) {

    struct RTP rtp_packet;
    rtp_packet.prepare();
    rtp_packet.set_seq_number(0x143f);
    rtp_packet.set_payload_type(96);
    rtp_packet.set_ssrc(0x01);
    rtp_packet.set_marker(false);
    rtp_packet.set_timestamp(0x00123456);

    EXPECT_EQ(rtp_packet.version(), 0x02);
    EXPECT_EQ(rtp_packet.padding(), false);
    EXPECT_EQ(rtp_packet.extension(), false);
    EXPECT_EQ(rtp_packet.csrc_count(), 0);

    EXPECT_EQ(rtp_packet.marker(), false);
    EXPECT_EQ(rtp_packet.payload_type(), 0x60 /* 96 */);
    EXPECT_EQ(rtp_packet.seq_number(), 0x143f);
    EXPECT_EQ(rtp_packet.timestamp(), 0x00123456);

    EXPECT_EQ(rtp_packet.ssrc(), 0x01);
    EXPECT_EQ(rtp_packet.header_size(), 12);

}

    
} // namespace test
} // namespace naivertc
