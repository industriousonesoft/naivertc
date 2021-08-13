#include "rtc/rtp_rtcp/rtcp_packets/tmmbn.hpp"
#include "rtc/rtp_rtcp/rtcp_packets/common_header.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace naivertc::rtcp;

namespace naivertc {
namespace test {
namespace {
constexpr uint32_t kSenderSsrc = 0x12345678;
constexpr uint32_t kRemoteSsrc = 0x23456789;
constexpr uint32_t kBitrateBps = 312000;
constexpr uint16_t kOverhead = 0x1fe;
constexpr uint8_t kPacket[] = {0x84, 205,  0x00, 0x04, 0x12, 0x34, 0x56,
                           0x78, 0x00, 0x00, 0x00, 0x00, 0x23, 0x45,
                           0x67, 0x89, 0x0a, 0x61, 0x61, 0xfe};
constexpr size_t kPacketSize = sizeof(kPacket);
}  // namespace

TEST(RtcpPacketTmmbnTest, Create) {
    Tmmbn tmmbn;
    tmmbn.set_sender_ssrc(kSenderSsrc);
    tmmbn.AddTmmbn(TmmbItem(kRemoteSsrc, kBitrateBps, kOverhead));

    BinaryBuffer packet = tmmbn.Build();
    EXPECT_EQ(packet.size(), kPacketSize);
    EXPECT_THAT(std::make_tuple(packet.data(), packet.size()), testing::ElementsAreArray(kPacket));
}

TEST(RtcpPacketTmmbnTest, Parse) {
    
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kPacket, kPacketSize));

    Tmmbn parsed;
    EXPECT_TRUE(parsed.Parse(common_header));

    EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
    ASSERT_EQ(1u, parsed.items().size());
    EXPECT_EQ(kRemoteSsrc, parsed.items().front().ssrc());
    EXPECT_EQ(kBitrateBps, parsed.items().front().bitrate_bps());
    EXPECT_EQ(kOverhead, parsed.items().front().packet_overhead());
}

TEST(RtcpPacketTmmbnTest, CreateAndParseWithoutItems) {
    Tmmbn tmmbn;
    tmmbn.set_sender_ssrc(kSenderSsrc);

    BinaryBuffer packet = tmmbn.Build();

    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(packet.data(), packet.size()));

    Tmmbn parsed;
    EXPECT_TRUE(parsed.Parse(common_header));

    EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
    EXPECT_THAT(parsed.items(), testing::IsEmpty());
}

TEST(RtcpPacketTmmbnTest, CreateAndParseWithTwoItems) {
    Tmmbn tmmbn;
    tmmbn.set_sender_ssrc(kSenderSsrc);
    tmmbn.AddTmmbn(TmmbItem(kRemoteSsrc, kBitrateBps, kOverhead));
    tmmbn.AddTmmbn(TmmbItem(kRemoteSsrc + 1, 4 * kBitrateBps, 40));

    BinaryBuffer packet = tmmbn.Build();
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(packet.data(), packet.size()));
    Tmmbn parsed;
    EXPECT_TRUE(parsed.Parse(common_header));

    EXPECT_EQ(kSenderSsrc, parsed.sender_ssrc());
    EXPECT_EQ(2u, parsed.items().size());
    EXPECT_EQ(kRemoteSsrc, parsed.items()[0].ssrc());
    EXPECT_EQ(kBitrateBps, parsed.items()[0].bitrate_bps());
    EXPECT_EQ(kOverhead, parsed.items()[0].packet_overhead());
    EXPECT_EQ(kRemoteSsrc + 1, parsed.items()[1].ssrc());
    EXPECT_EQ(4 * kBitrateBps, parsed.items()[1].bitrate_bps());
    EXPECT_EQ(40U, parsed.items()[1].packet_overhead());
}

TEST(RtcpPacketTmmbnTest, ParseFailsOnTooSmallPacket) {
    const uint8_t kSmallPacket[] = {0x84, 205,  0x00, 0x01,
                                    0x12, 0x34, 0x56, 0x78};
    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kSmallPacket, sizeof(kSmallPacket)));
    Tmmbn tmmbn;
    EXPECT_FALSE(tmmbn.Parse(common_header));
}

TEST(RtcpPacketTmmbnTest, ParseFailsOnUnAlignedPacket) {
    const uint8_t kUnalignedPacket[] = {0x84, 205,  0x00, 0x03, 0x12, 0x34,
                                        0x56, 0x78, 0x00, 0x00, 0x00, 0x00,
                                        0x23, 0x45, 0x67, 0x89};

    CommonHeader common_header;
    EXPECT_TRUE(common_header.Parse(kUnalignedPacket, sizeof(kUnalignedPacket)));
    Tmmbn tmmbn;
    EXPECT_FALSE(tmmbn.Parse(common_header));
}
    
} // namespace test
} // namespace naivertc
