#include "rtc/rtp_rtcp/rtcp/rtcp_packets/remb.hpp"
#include "rtc/rtp_rtcp/rtcp/rtcp_packets/common_header.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {

namespace {
const uint32_t kSenderSsrc = 0x12345678;
const uint32_t kRemoteSsrcs[] = {0x23456789, 0x2345678a, 0x2345678b};
const uint32_t kBitrateBps = 0x3fb93 * 2;  // 522022;
const int64_t kBitrateBps64bit = int64_t{0x3fb93} << 30;
// Manually created Fir packet matching constants above.
const uint8_t kPacket[] = {0x8f, 206,  0x00, 0x07, 0x12, 0x34, 0x56, 0x78,
                           0x00, 0x00, 0x00, 0x00, 'R',  'E',  'M',  'B',
                           0x03, 0x07, 0xfb, 0x93, 0x23, 0x45, 0x67, 0x89,
                           0x23, 0x45, 0x67, 0x8a, 0x23, 0x45, 0x67, 0x8b};
const size_t kPacketLength = sizeof(kPacket);
} // namespace

TEST(RtcpRembTest, Create) {
    Remb remb;
    remb.set_sender_ssrc(kSenderSsrc);
    remb.set_ssrcs(std::vector<uint32_t>(std::begin(kRemoteSsrcs), std::end(kRemoteSsrcs)));
    remb.set_bitrate_bps(kBitrateBps);

    EXPECT_EQ(kSenderSsrc, remb.sender_ssrc());
    EXPECT_EQ(kPacketLength, remb.PacketSize());
    EXPECT_EQ(3u, remb.ssrcs().size());

    BinaryBuffer raw = remb.Build();
    EXPECT_THAT(raw, testing::ElementsAreArray(kPacket));
}

TEST(RtcpRembTest, Parse) {
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kPacket, kPacketLength));

    EXPECT_EQ(common_header.feedback_message_type(), Psfb::kAfbMessageType);
    EXPECT_EQ(common_header.type(), Psfb::kPacketType);
    EXPECT_EQ(common_header.payload_size(), kPacketLength - RtcpPacket::kRtcpCommonHeaderSize);

    Remb remb;
    EXPECT_TRUE(remb.Parse(common_header));
    EXPECT_EQ(kSenderSsrc, remb.sender_ssrc());
    EXPECT_EQ(kBitrateBps, remb.bitrate_bps());
    EXPECT_THAT(remb.ssrcs(), testing::ElementsAreArray(kRemoteSsrcs));
}

TEST(RtcpRembTest, ParseFailsWhenUniqueIdentifierIsNotRemb) {
    uint8_t packet[kPacketLength];
    memcpy(packet, kPacket, kPacketLength);
    packet[15] = 'A'; // Swap 'B' -> 'A' in the 'REMB' unique identifier

    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(packet, kPacketLength));

    Remb remb;
    EXPECT_FALSE(remb.Parse(common_header));
}
    
} // namespace test
} // namespace naivertc

